# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch official release inputs without accepting them into the target catalog."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlsplit
from urllib.request import build_opener, HTTPRedirectHandler, Request

from .images import safe_path, write_json

# The original upstream was renamed. Repository identity survives that rename;
# a future, unreviewed transfer must not silently change our input provenance.
REPOSITORY_ID = 1119492074
REPOSITORY_NAMES = frozenset({'ReSukiSU/ReSukiSU', 'Baka-SU/BakaSU'})
API_ROOT = f'https://api.github.com/repositories/{REPOSITORY_ID}'
MAX_METADATA = 8 << 20
MAX_ARTIFACT = 512 << 20
SOCKET_TIMEOUT = 20
TRANSFER_SECONDS = 300


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _positive_id(value):
    return type(value) is int and value > 0


def _timestamp(value):
    _require(isinstance(value, str), 'Missing publication timestamp')
    try:
        result = datetime.fromisoformat(value.replace('Z', '+00:00'))
    except ValueError as exc:
        raise ValueError('Invalid publication timestamp') from exc
    _require(result.tzinfo is not None, 'Publication timestamp must include a timezone')
    return result


def _allowed_url(url, kind):
    parsed = urlsplit(url)
    _require(parsed.scheme == 'https' and not parsed.username and not parsed.password
             and parsed.port in (None, 443) and not parsed.fragment,
             'Only HTTPS public upstream URLs are allowed')
    hosts = {
        'api': {'api.github.com'},
        'apk': {'github.com', 'release-assets.githubusercontent.com', 'objects.githubusercontent.com'},
        'source': {'codeload.github.com'},
    }
    _require(parsed.hostname in hosts[kind], 'Unexpected upstream download host')
    if parsed.hostname == 'api.github.com':
        prefixes = [f'/repositories/{REPOSITORY_ID}']
        prefixes.extend(f'/repos/{name}' for name in REPOSITORY_NAMES)
        _require(any(parsed.path == p or parsed.path.startswith(p + '/') for p in prefixes),
                 'Unexpected upstream API repository')
    if parsed.hostname in {'github.com', 'codeload.github.com'}:
        suffix = '/releases/download/' if kind == 'apk' else '/tar.gz/'
        _require(any(parsed.path.startswith('/' + name + suffix) for name in REPOSITORY_NAMES),
                 'Unexpected upstream artifact repository')


class _Redirects(HTTPRedirectHandler):
    def __init__(self, kind):
        super().__init__()
        self.kind = kind

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        _allowed_url(newurl, self.kind)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def _open(url, kind):
    _allowed_url(url, kind)
    headers = {'User-Agent': 'mumu-resukisu-release-fetch', 'Accept-Encoding': 'identity'}
    if kind == 'api':
        headers.update({'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
    # No local credentials are loaded or forwarded to public release hosts.
    response = build_opener(_Redirects(kind)).open(Request(url, headers=headers), timeout=SOCKET_TIMEOUT)
    try:
        _allowed_url(response.geturl(), kind)
        _require(response.status == 200, 'Unexpected upstream response status')
        _require(response.headers.get('Content-Encoding', 'identity') == 'identity',
                 'Unexpected encoded transfer')
    except BaseException:
        response.close()
        raise
    return response


def _json(url):
    deadline = time.monotonic() + TRANSFER_SECONDS
    parts = []
    size = 0
    with _open(url, 'api') as response:
        while True:
            _require(time.monotonic() <= deadline, 'Metadata transfer time limit exceeded')
            chunk = response.read1(1 << 16)
            if not chunk:
                break
            size += len(chunk)
            _require(size <= MAX_METADATA, 'Upstream metadata exceeds limit')
            parts.append(chunk)
    return json.loads(b''.join(parts))


def _repository():
    repo = _json(API_ROOT)
    _require(isinstance(repo, dict) and type(repo.get('id')) is int
             and repo['id'] == REPOSITORY_ID and repo.get('full_name') in REPOSITORY_NAMES,
             'Upstream repository identity changed; maintainer review required')
    return repo['full_name']


def _latest(channel):
    if channel == 'stable':
        try:
            result = _json(API_ROOT + '/releases/latest')
        except HTTPError as exc:
            if exc.code == 404:
                raise ValueError('No stable release is published. Use --channel prerelease explicitly to include prereleases.') from None
            raise
        _require(isinstance(result, dict) and result.get('prerelease') is False
                 and result.get('draft') is False, 'Latest stable metadata is not a public stable release')
        return result
    _require(channel == 'prerelease', 'Unsupported release channel')
    found = []
    # Publication date, not API list order or commit creation date, defines this
    # channel. Never claim latest if a bounded traversal could be incomplete.
    for page in range(1, 21):
        batch = _json(API_ROOT + f'/releases?per_page=100&page={page}')
        _require(isinstance(batch, list) and len(batch) <= 100, 'Invalid release list')
        for release in batch:
            _require(isinstance(release, dict) and type(release.get('draft')) is bool
                     and type(release.get('prerelease')) is bool, 'Invalid release flags')
            if not release['draft']:
                _timestamp(release.get('published_at'))
                _require(_positive_id(release.get('id')), 'Invalid release identity')
                found.append(release)
        if len(batch) < 100:
            break
    else:
        raise ValueError('Release pagination limit reached; latest release could not be established')
    _require(bool(found), 'No public release is published')
    return max(found, key=lambda item: (_timestamp(item['published_at']), item['id']))


def _resolve_tag(tag):
    ref = _json(API_ROOT + '/git/ref/tags/' + quote(tag, safe=''))
    _require(isinstance(ref, dict) and ref.get('ref') == 'refs/tags/' + tag,
             'Release tag identity mismatch')
    obj = ref.get('object')
    seen = set()
    for _ in range(8):
        _require(isinstance(obj, dict) and isinstance(obj.get('sha'), str)
                 and re.fullmatch('[0-9a-f]{40}', obj['sha']), 'Invalid release tag object')
        digest = obj['sha']
        if obj.get('type') == 'commit':
            return digest
        _require(obj.get('type') == 'tag' and digest not in seen, 'Invalid or cyclic annotated tag')
        seen.add(digest)
        annotated = _json(API_ROOT + '/git/tags/' + digest)
        _require(isinstance(annotated, dict) and annotated.get('sha') == digest, 'Annotated tag identity mismatch')
        obj = annotated.get('object')
    raise ValueError('Annotated tag depth exceeds limit')


def _selection(release, repository, channel):
    _require(isinstance(release, dict) and _positive_id(release.get('id'))
             and release.get('draft') is False and type(release.get('prerelease')) is bool,
             'Invalid public release identity')
    tag = release.get('tag_name')
    _require(isinstance(tag, str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,127}', tag),
             'Unsupported release tag spelling')
    published = release.get('published_at')
    _timestamp(published)
    assets = release.get('assets')
    _require(isinstance(assets, list), 'Missing release assets')
    candidates = []
    architecture = None
    for arch in ('x86_64', 'universal'):
        candidates = [a for a in assets if isinstance(a, dict) and isinstance(a.get('name'), str)
                      and a['name'].endswith('-' + arch + '-release.apk')]
        if candidates:
            architecture = arch
            break
    _require(len(candidates) == 1, 'Exactly one x86_64 or fallback universal release APK is required')
    asset = candidates[0]
    name = asset['name']
    _require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,199}\.apk', name)
             and _positive_id(asset.get('id')) and type(asset.get('size')) is int
             and 0 < asset['size'] <= MAX_ARTIFACT and asset.get('state') == 'uploaded',
             'Invalid uploaded APK asset')
    digest = asset.get('digest')
    _require(isinstance(digest, str) and re.fullmatch('sha256:[0-9a-f]{64}', digest),
             'Release APK has no supported upstream SHA-256 digest; manual review required')
    url = f'https://github.com/{repository}/releases/download/{quote(tag, safe="")}/{quote(name, safe="")}'
    _require(asset.get('browser_download_url') == url, 'APK URL does not match official release metadata')
    commit = _resolve_tag(tag)
    return {
        'schemaVersion': 1, 'repositoryId': REPOSITORY_ID, 'repository': repository,
        'channel': channel, 'releaseId': release['id'], 'tag': tag,
        'prerelease': release['prerelease'], 'publishedAt': published,
        'releaseUrl': f'https://github.com/{repository}/releases/tag/{quote(tag, safe="")}',
        'sourceCommit': commit,
        'apk': {'assetId': asset['id'], 'file': name, 'bytes': asset['size'],
                'architecture': architecture, 'url': url, 'upstreamSha256': digest[7:]},
        'source': {'file': 'source-' + commit + '.tar.gz',
                   'url': f'https://codeload.github.com/{repository}/tar.gz/{commit}'},
        'apkSignatureVerified': False, 'sourceBinaryReproducibilityProven': False,
        'compatibilityValidated': False, 'catalogUpdated': False, 'deviceWritten': False,
    }


def discover(channel='stable'):
    repository = _repository()
    result = _selection(_latest(channel), repository, channel)
    result['checkedAt'] = datetime.now(timezone.utc).isoformat()
    result['downloaded'] = False
    return result


def _download(url, target, kind, expected_size=None, expected_sha=None):
    deadline = time.monotonic() + TRANSFER_SECONDS
    digest = hashlib.sha256()
    size = 0
    with _open(url, kind) as response, target.open('xb') as output:
        length = response.headers.get('Content-Length')
        if length is not None:
            _require(length.isdecimal() and 0 < int(length) <= MAX_ARTIFACT,
                     'Invalid download Content-Length')
            _require(expected_size is None or int(length) == expected_size, 'Download size differs from release metadata')
        while True:
            _require(time.monotonic() <= deadline, 'Download total time limit exceeded')
            chunk = response.read1(1 << 20)
            if not chunk:
                break
            size += len(chunk)
            _require(size <= MAX_ARTIFACT and (expected_size is None or size <= expected_size),
                     'Download exceeds declared size or limit')
            output.write(chunk)
            digest.update(chunk)
        _require(size > 0 and (length is None or size == int(length)), 'Truncated or empty download')
        _require(expected_size is None or size == expected_size, 'APK byte count mismatch')
        actual = digest.hexdigest()
        _require(expected_sha is None or actual == expected_sha, 'APK SHA-256 mismatch')
        return {'sha256': actual, 'bytes': size,
                'upstreamDigestVerified': expected_sha is not None}


def fetch(output, channel='stable'):
    output = safe_path(output)
    _require(not output.exists(), 'Output must be a new directory')
    result = discover(channel)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Only this private staging directory is cleaned up on failure. An existing
    # output is never replaced, and a manifest is published only after both files.
    staging = Path(tempfile.mkdtemp(prefix='.release-fetch-', dir=output.parent))
    try:
        apk = result['apk']
        apk_result = _download(apk['url'], staging / apk['file'], 'apk', apk['bytes'], apk['upstreamSha256'])
        source = result['source']
        source_result = _download(source['url'], staging / source['file'], 'source')
        with (staging / source['file']).open('rb') as source_file:
            _require(source_file.read(2) == b'\x1f\x8b', 'Source response is not a gzip archive')
        # A mutable tag or replaced asset must not be mixed across this transfer.
        repository = _repository()
        current = _selection(_json(API_ROOT + '/releases/' + str(result['releaseId'])), repository, channel)
        for field in ('repository', 'releaseId', 'tag', 'prerelease', 'publishedAt', 'sourceCommit', 'apk', 'source'):
            _require(current[field] == result[field], 'Release changed during download; retry with fresh metadata')
        apk.update(apk_result)
        source.update(source_result)
        result.update({'downloaded': True, 'completedAt': datetime.now(timezone.utc).isoformat(),
                       'sourceIdentity': 'GitHub release tag resolved to commit; archive is downloaded by commit without extraction',
                       'sourceApkAssociationVerified': False,
                       'nextStep': 'Verify APK signature and review source/release association before proposing a catalog entry'})
        write_json(staging / 'release.json', result)
        safe_path(output)
        _require(not output.exists(), 'Output appeared during download')
        staging.rename(output)
        return result
    finally:
        if staging.exists():
            # No downloaded archive is extracted, so the directory contains only
            # files created by this function, not attacker-controlled paths.
            safe_path(staging)
            shutil.rmtree(staging)


def add_arguments(parser):
    parser.add_argument('--channel', choices=('stable', 'prerelease'), default='stable',
                        help='stable requires a stable release; prerelease includes public prereleases')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true', help='Query release identity without downloading')
    mode.add_argument('--output', type=Path, help='Download into a new local directory')


def run(args):
    return discover(args.channel) if args.check else fetch(args.output, args.channel)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    add_arguments(parser)
    args = parser.parse_args(argv)
    try:
        print(json.dumps(run(args), ensure_ascii=False, indent=2))
    except HTTPError as exc:
        parser.exit(1, f'Upstream HTTP {exc.code}; check release availability or public API rate limits.\n')
    except (URLError, TimeoutError, OSError, ValueError) as exc:
        parser.exit(1, f'Release fetch failed: {exc}\n')


if __name__ == '__main__':
    main()
