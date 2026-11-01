# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
from contextlib import redirect_stderr
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from urllib.error import HTTPError

from mumu_resukisu import releases as r

REPO = 'Baka-SU/BakaSU'
COMMIT = 'a' * 40
APK = b'an APK whose signature is deliberately not tested here'
SOURCE = gzip.compress(b'source archive fixture')


def release(tag='v1', published='2026-01-01T00:00:00Z', prerelease=True, identifier=1):
    name = f'ReSukiSU_{tag}_123-x86_64-release.apk'
    return {'id': identifier, 'draft': False, 'prerelease': prerelease, 'tag_name': tag,
            'published_at': published,
            'assets': [{'id': identifier * 10, 'name': name, 'size': len(APK), 'state': 'uploaded',
                        'digest': 'sha256:' + hashlib.sha256(APK).hexdigest(),
                        'browser_download_url': f'https://github.com/{REPO}/releases/download/{tag}/{name}'}]}


def metadata(url):
    if url == r.API_ROOT:
        return {'id': r.REPOSITORY_ID, 'full_name': REPO}
    if '/git/ref/tags/' in url:
        return {'ref': 'refs/tags/' + url.rsplit('/', 1)[-1],
                'object': {'type': 'commit', 'sha': COMMIT}}
    if '/releases?' in url:
        return [release()]
    if '/releases/' in url:
        return release()
    raise AssertionError(url)


class Response(io.BytesIO):
    def __init__(self, data, length=None):
        super().__init__(data)
        self.headers = {} if length is None else {'Content-Length': str(length)}


def download_response(url, kind):
    return Response(APK if kind == 'apk' else SOURCE)


class ReleaseTests(unittest.TestCase):
    def test_default_channel_requires_explicit_mode(self):
        p = argparse.ArgumentParser()
        r.add_arguments(p)
        self.assertEqual(p.parse_args(['--check']).channel, 'stable')
        with self.assertRaises(SystemExit), redirect_stderr(io.StringIO()):
            p.parse_args(['--check', '--output', 'new'])

    def test_stable_missing_never_falls_back(self):
        missing = HTTPError(r.API_ROOT, 404, 'missing', {}, None)
        with patch.object(r, '_json', side_effect=missing) as api:
            with self.assertRaisesRegex(ValueError, 'No stable release'):
                r._latest('stable')
            self.assertEqual(api.call_count, 1)

    def test_stable_rejects_prerelease(self):
        with patch.object(r, '_json', return_value=release()):
            with self.assertRaisesRegex(ValueError, 'stable metadata'):
                r._latest('stable')
        stable = release(prerelease=False)
        with patch.object(r, '_json', return_value=stable):
            self.assertEqual(r._latest('stable'), stable)

    def test_latest_uses_publication_date_and_ignores_drafts(self):
        older = release('v2', '2025-01-01T00:00:00Z', identifier=9)
        newer = release('v1', '2026-01-01T00:00:00Z', prerelease=False)
        draft = release('v3', '2027-01-01T00:00:00Z')
        draft['draft'] = True
        with patch.object(r, '_json', return_value=[draft, older, newer]):
            self.assertEqual(r._latest('prerelease')['tag_name'], 'v1')

    def test_pagination_is_complete_or_fails_closed(self):
        batch = [release(identifier=i + 1) for i in range(100)]
        winner = release('v9', '2027-01-01T00:00:00Z')
        with patch.object(r, '_json', side_effect=[batch, [winner]]) as api:
            self.assertEqual(r._latest('prerelease')['tag_name'], 'v9')
            self.assertIn('page=2', api.call_args.args[0])
        with patch.object(r, '_json', return_value=batch):
            with self.assertRaisesRegex(ValueError, 'pagination limit'):
                r._latest('prerelease')

    def test_repository_identity_and_reviewed_rename(self):
        for name in r.REPOSITORY_NAMES:
            with patch.object(r, '_json', return_value={'id': r.REPOSITORY_ID, 'full_name': name}):
                self.assertEqual(r._repository(), name)
        for repo in ({'id': 12, 'full_name': REPO}, {'id': r.REPOSITORY_ID, 'full_name': 'other/project'}):
            with patch.object(r, '_json', return_value=repo):
                with self.assertRaisesRegex(ValueError, 'identity changed'):
                    r._repository()

    def test_annotated_tag_is_dereferenced_to_commit(self):
        tag_sha = 'b' * 40
        with patch.object(r, '_json', side_effect=[
                {'ref': 'refs/tags/v1', 'object': {'type': 'tag', 'sha': tag_sha}},
                {'sha': tag_sha, 'object': {'type': 'commit', 'sha': COMMIT}}]):
            self.assertEqual(r._resolve_tag('v1'), COMMIT)

    def test_tag_cycles_and_non_commit_objects_rejected(self):
        for kind in ('tag', 'tree'):
            obj = {'type': kind, 'sha': COMMIT}
            with patch.object(r, '_json', side_effect=[{'ref': 'refs/tags/v1', 'object': obj},
                                                       {'sha': COMMIT, 'object': obj}]):
                with self.assertRaises(ValueError):
                    r._resolve_tag('v1')

    def test_query_is_inert_and_records_commit_without_compatibility_claim(self):
        with patch.object(r, '_json', side_effect=metadata), patch.object(r, '_download') as download:
            result = r.discover('prerelease')
        self.assertEqual(result['sourceCommit'], COMMIT)
        self.assertFalse(result['downloaded'])
        self.assertFalse(result['apkSignatureVerified'])
        self.assertFalse(result['compatibilityValidated'])
        self.assertFalse(result['catalogUpdated'])
        self.assertFalse(result['deviceWritten'])
        download.assert_not_called()

    def test_asset_requires_unique_architecture_digest_and_official_url(self):
        mutations = [lambda a: a.update(digest=None), lambda a: a.update(size=True),
                     lambda a: a.update(browser_download_url='https://example.com/payload.apk'),
                     lambda a: a.update(state='new'), lambda a: a.update(name='../x-x86_64-release.apk')]
        for change in mutations:
            value = release()
            change(value['assets'][0])
            with self.subTest(value=value), patch.object(r, '_json', side_effect=metadata):
                with self.assertRaises(ValueError):
                    r._selection(value, REPO, 'prerelease')
        value = release()
        value['assets'] *= 2
        with self.assertRaises(ValueError):
            r._selection(value, REPO, 'prerelease')

    def test_universal_fallback_is_explicit(self):
        value = release()
        asset = value['assets'][0]
        asset['name'] = asset['name'].replace('x86_64', 'universal')
        asset['browser_download_url'] = asset['browser_download_url'].replace('x86_64', 'universal')
        with patch.object(r, '_json', side_effect=metadata):
            self.assertEqual(r._selection(value, REPO, 'prerelease')['apk']['architecture'], 'universal')

    def test_redirect_allowlist_rejects_foreign_host_repo_and_http(self):
        for url, kind in [('http://github.com/a/b', 'apk'),
                          ('https://example.com/file', 'apk'),
                          ('https://github.com/other/project/releases/download/v1/file.apk', 'apk'),
                          ('https://api.github.com/repos/other/project/releases', 'api'),
                          ('https://api.github.com/repositories/11194920740/releases', 'api'),
                          ('https://user@codeload.github.com/Baka-SU/BakaSU/tar.gz/abc', 'source')]:
            with self.subTest(url=url), self.assertRaises(ValueError):
                r._allowed_url(url, kind)
        r._allowed_url('https://release-assets.githubusercontent.com/a?temporary=value', 'apk')

    def test_redirect_handler_rejects_before_following(self):
        with self.assertRaises(ValueError):
            r._Redirects('source').redirect_request(None, None, 302, '', {}, 'https://example.com/archive')

    def test_download_detects_truncation_oversize_and_hash_mismatch(self):
        cases = [(APK[:-1], len(APK), hashlib.sha256(APK).hexdigest()),
                 (APK + b'!', len(APK), hashlib.sha256(APK).hexdigest()),
                 (APK, len(APK), '0' * 64)]
        for data, size, digest in cases:
            with self.subTest(data=data), tempfile.TemporaryDirectory() as tmp:
                with patch.object(r, '_open', return_value=Response(data)):
                    with self.assertRaises(ValueError):
                        r._download('https://unused', Path(tmp) / 'file', 'apk', size, digest)

    def test_download_checks_header_size_and_deadline(self):
        with tempfile.TemporaryDirectory() as tmp:
            with patch.object(r, '_open', return_value=Response(APK, length=1000)):
                with self.assertRaisesRegex(ValueError, 'size differs'):
                    r._download('https://unused', Path(tmp) / 'file1', 'apk', len(APK))
            with patch.object(r, '_open', return_value=Response(APK)), patch.object(r.time, 'monotonic', side_effect=[0, 9999]):
                with self.assertRaisesRegex(ValueError, 'time limit'):
                    r._download('https://unused', Path(tmp) / 'file2', 'apk')

    def test_fetch_publishes_only_complete_pair_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(r, '_json', side_effect=metadata), \
                patch.object(r, '_open', side_effect=download_response):
            output = Path(tmp) / 'release'
            result = r.fetch(output, 'prerelease')
            manifest = json.loads((output / 'release.json').read_text())
            self.assertEqual(result, manifest)
            self.assertEqual((output / result['apk']['file']).read_bytes(), APK)
            self.assertEqual((output / result['source']['file']).read_bytes(), SOURCE)
            self.assertTrue(result['apk']['upstreamDigestVerified'])
            self.assertFalse(result['source']['upstreamDigestVerified'])
            self.assertFalse(result['sourceApkAssociationVerified'])
            self.assertEqual(list(Path(tmp).iterdir()), [output])

    def test_failure_cleans_partial_pair_without_publishing_output(self):
        for failure in ('network', 'tag_changed', 'asset_changed', 'source_html'):
            calls = 0

            def changing_metadata(url):
                nonlocal calls
                value = metadata(url)
                if '/releases/1' in url and failure == 'asset_changed':
                    value['assets'][0]['id'] = 99
                if '/git/ref/tags/' in url:
                    calls += 1
                    if calls == 2 and failure == 'tag_changed':
                        value['object']['sha'] = 'f' * 40
                return value

            def response(url, kind):
                if kind == 'source' and failure == 'network':
                    raise TimeoutError('test timeout')
                if kind == 'source' and failure == 'source_html':
                    return Response(b'<html>error</html>')
                return download_response(url, kind)

            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as tmp, \
                    patch.object(r, '_json', side_effect=changing_metadata), patch.object(r, '_open', side_effect=response):
                with self.assertRaises((TimeoutError, ValueError)):
                    r.fetch(Path(tmp) / 'release', 'prerelease')
                self.assertEqual(list(Path(tmp).iterdir()), [])

    def test_existing_output_is_preserved_before_network_access(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(r, '_json') as api:
            output = Path(tmp)
            marker = output / 'keep'
            marker.write_text('user data')
            with self.assertRaisesRegex(ValueError, 'new directory'):
                r.fetch(output)
            self.assertEqual(marker.read_text(), 'user data')
            api.assert_not_called()

    def test_output_appearing_during_download_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(r, '_json', side_effect=metadata):
            output = Path(tmp) / 'release'

            def response(url, kind):
                if kind == 'source':
                    output.mkdir()
                    (output / 'keep').write_text('new user data')
                return download_response(url, kind)

            with patch.object(r, '_open', side_effect=response):
                with self.assertRaisesRegex(ValueError, 'appeared'):
                    r.fetch(output, 'prerelease')
            self.assertEqual((output / 'keep').read_text(), 'new user data')
            self.assertEqual(list(Path(tmp).iterdir()), [output])

    def test_redirected_output_parent_is_rejected_before_network(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(r, '_json') as api:
            real = Path(tmp) / 'real'
            real.mkdir()
            link = Path(tmp) / 'link'
            if os.name == 'nt':
                subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(link), str(real)],
                               check=True, capture_output=True, timeout=10)
            else:
                link.symlink_to(real, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'Symbolic link|Reparse point'):
                r.fetch(link / 'output')
            self.assertEqual(list(real.iterdir()), [])
            api.assert_not_called()

    def test_metadata_transfer_is_bounded(self):
        with patch.object(r, '_open', return_value=Response(b' ' * 30)), patch.object(r, 'MAX_METADATA', 10):
            with self.assertRaisesRegex(ValueError, 'metadata exceeds'):
                r._json('https://unused')


if __name__ == '__main__':
    unittest.main()
