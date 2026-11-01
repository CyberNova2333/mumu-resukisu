#!/usr/bin/env python3
"""Build one offline, fixed-slot test candidate; never access a device or disk image."""
from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import struct
import sys
from datetime import datetime, timezone
import zlib


EXPECTED_KERNEL = "48e7b624b3652ccbc215e25b24432bab052081b55aa806e77c28fe4c8bd9a685"
EXPECTED_ELF = "9e7bb12dd0248829556a416dc7a768918f91a534f72cfef3a36c385c1ae8bfad"
MAGIC = 0x184C2102
BLOCK = 8 << 20


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def decode_block_independently(data: bytes) -> bytes:
    """Separate pure-Python LZ4 block decoder, without using liblz4."""
    result = bytearray()
    cursor = 0

    def length(base: int) -> int:
        nonlocal cursor
        if base == 15:
            while True:
                assert cursor < len(data), "truncated LZ4 length"
                part = data[cursor]
                cursor += 1
                base += part
                if part != 255:
                    break
        return base

    while cursor < len(data):
        token = data[cursor]
        cursor += 1
        literals = length(token >> 4)
        assert cursor + literals <= len(data), "truncated LZ4 literals"
        result.extend(data[cursor:cursor + literals])
        cursor += literals
        if cursor == len(data):
            break
        assert cursor + 2 <= len(data), "truncated LZ4 match offset"
        distance = struct.unpack_from("<H", data, cursor)[0]
        cursor += 2
        assert 0 < distance <= len(result), "invalid LZ4 match distance"
        count = length(token & 15) + 4
        assert len(result) + count <= BLOCK, "LZ4 block output overflow"
        pattern = bytes(result[-distance:])
        result.extend((pattern * ((count + distance - 1) // distance))[:count])
    assert len(result) <= BLOCK
    return bytes(result)


def split_stream(payload: bytes, *, permit_zero_padding: bool) -> tuple[list[bytes], bytes]:
    assert u32(payload, 0) == MAGIC
    expected = u32(payload, len(payload) - 4)
    cursor = 4
    blocks = []
    raw = bytearray()
    while cursor < len(payload) - 4:
        assert cursor + 4 <= len(payload) - 4
        size = u32(payload, cursor)
        cursor += 4
        if size == 0:
            assert permit_zero_padding
            assert not any(payload[cursor:len(payload) - 4]), "nonzero trailing padding"
            break
        assert size != MAGIC, "unexpected concatenated frame"
        assert 0 < size <= BLOCK + BLOCK // 255 + 16
        assert cursor + size <= len(payload) - 4
        block = payload[cursor:cursor + size]
        cursor += size
        decoded = decode_block_independently(block)
        blocks.append(block)
        raw.extend(decoded)
        assert len(raw) <= expected
    assert len(raw) == expected, "incomplete or excessive decompressed output"
    return blocks, bytes(raw)


def main() -> None:
    if not __debug__:
        raise RuntimeError("do not disable validation with python -O")
    assert sys.platform.startswith("linux"), "run under WSL using system liblz4"
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kernel",type=Path,required=True)
    ap.add_argument("--elf",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    OUTPUT=args.output.resolve()
    assert not OUTPUT.exists(), "output must be a new directory"
    original_kernel = args.kernel.read_bytes()
    original_elf = args.elf.read_bytes()
    assert sha(original_kernel) == EXPECTED_KERNEL
    assert sha(original_elf) == EXPECTED_ELF
    old_text = b"stack_depot_disable=on cgroup_disable=pressure"
    addition = b" initcall_blacklist=kernelsu_init"
    new_text = old_text + addition
    offset = 0x1ef82f0
    buffer = original_elf[offset:offset + 2048]
    assert len(buffer) == 2048 and buffer.startswith(old_text + b"\0")
    assert sha(buffer) == "182cc032a3a47adcdb3d0d397b9fea0892b5bee09edf3c71e9b69aaae0504e53"
    assert not any(buffer[len(old_text):])
    assert len(new_text) == 79 and len(new_text) + 1 < len(buffer)
    changed_elf = bytearray(original_elf)
    changed_elf[offset:offset + len(new_text)] = new_text
    cmdline_changed_indices = [i for i, (a, b) in enumerate(zip(original_elf, changed_elf)) if a != b]
    assert cmdline_changed_indices == list(range(offset + len(old_text), offset + len(new_text)))
    assert len(cmdline_changed_indices) == len(addition) == 33

    # A section-header-free executable is valid ELF. Drop only non-loaded analysis
    # metadata to reclaim compressed space, preserving byte offsets and the complete
    # appended relocation table. Never strip or truncate a PT_LOAD/NOTE segment.
    assert original_elf[:6] == b"\x7fELF\x02\x01"
    phoff, shoff = struct.unpack_from("<QQ", original_elf, 32)
    phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from("<HHHHH", original_elf, 54)
    assert (phoff, phentsize, phnum) == (0x40, 56, 5)
    assert (shoff, shentsize, shnum, shstrndx) == (0x1FF91C0, 64, 40, 39)
    phdrs = [struct.unpack_from("<IIQQQQQQ", original_elf, phoff + i * phentsize) for i in range(phnum)]
    string_section = struct.unpack_from("<IIQQQQIIQQ", original_elf, shoff + shstrndx * shentsize)
    assert string_section[1] == 3 and string_section[2] == 0
    string_start, string_size = string_section[4:6]
    strip_start = string_start
    strip_end = shoff + shnum * shentsize
    assert (string_start, string_size, strip_end) == (0x1FF9000, 0x1BA, 0x1FF9BC0)
    assert string_start + string_size <= shoff
    assert not any(original_elf[string_start + string_size:shoff])
    for phdr in phdrs:
        start, end = phdr[2], phdr[2] + phdr[5]
        assert end <= strip_start or start >= strip_end, "metadata intersects a program segment"
    assert max(p[2] + p[5] for p in phdrs if p[0] == 1) == strip_start
    changed_elf[strip_start:strip_end] = bytes(strip_end - strip_start)
    struct.pack_into("<Q", changed_elf, 40, 0)  # e_shoff
    struct.pack_into("<H", changed_elf, 60, 0)  # e_shnum
    struct.pack_into("<H", changed_elf, 62, 0)  # e_shstrndx
    changed_elf = bytes(changed_elf)
    changed_indices = [i for i, (a, b) in enumerate(zip(original_elf, changed_elf)) if a != b]
    permitted_elf_ranges = [(40, 48), (60, 64), (offset + len(old_text), offset + len(new_text)), (strip_start, strip_end)]
    assert all(any(start <= i < end for start, end in permitted_elf_ranges) for i in changed_indices)
    assert changed_elf[strip_end:] == original_elf[strip_end:], "appended relocation bytes changed"
    for phdr in phdrs:
        start, end = phdr[2], phdr[2] + phdr[5]
        before, after = original_elf[start:end], changed_elf[start:end]
        if start <= offset < end:
            expected = bytearray(before)
            expected[offset - start:offset - start + len(new_text)] = new_text
            assert after == expected
        else:
            assert before == after, "unexpected program segment modification"

    protected_start = (original_kernel[0x1F1] + 1) * 512
    payload_start = protected_start + u32(original_kernel, 0x248)
    payload_length = u32(original_kernel, 0x24C)
    payload_end = payload_start + payload_length
    assert protected_start == 20480 and payload_start == 21177 and payload_length == 12282201
    assert len(original_kernel) == 12362752
    assert u32(original_kernel, len(original_kernel) - 4) == (zlib.crc32(original_kernel[:-4]) ^ 0xFFFFFFFF)
    original_payload = original_kernel[payload_start:payload_end]
    original_blocks, decoded_original = split_stream(original_payload, permit_zero_padding=False)
    assert decoded_original == original_elf
    assert len(original_blocks) == 5
    assert u32(original_payload, len(original_payload) - 4) == len(original_elf)

    library_name = ctypes.util.find_library("lz4")
    assert library_name
    library = ctypes.CDLL(library_name)
    library.LZ4_compressBound.argtypes = [ctypes.c_int]
    library.LZ4_compressBound.restype = ctypes.c_int
    library.LZ4_compress_HC.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    library.LZ4_compress_HC.restype = ctypes.c_int
    library.LZ4_decompress_safe.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    library.LZ4_decompress_safe.restype = ctypes.c_int
    library.LZ4_versionNumber.restype = ctypes.c_int

    candidate_blocks = list(original_blocks)
    changed_block_indices = sorted({i // BLOCK for i in changed_indices})
    assert changed_block_indices == [0, 3]
    for changed_block_index in changed_block_indices:
        block_bytes = changed_elf[changed_block_index * BLOCK:(changed_block_index + 1) * BLOCK]
        bound = library.LZ4_compressBound(len(block_bytes))
        destination = ctypes.create_string_buffer(bound)
        size = library.LZ4_compress_HC(block_bytes, destination, len(block_bytes), bound, 12)
        assert 0 < size <= bound
        compressed = destination.raw[:size]
        assert decode_block_independently(compressed) == block_bytes
        check = ctypes.create_string_buffer(len(block_bytes))
        assert library.LZ4_decompress_safe(compressed, check, size, len(block_bytes)) == len(block_bytes)
        assert check.raw == block_bytes
        candidate_blocks[changed_block_index] = compressed
    stream = struct.pack("<I", MAGIC) + b"".join(struct.pack("<I", len(b)) + b for b in candidate_blocks)
    padding_bytes = payload_length - 4 - len(stream)
    assert padding_bytes >= 4, "candidate must fit original payload slot with an explicit zero-chunk terminator"
    candidate_payload = stream + bytes(padding_bytes) + original_payload[-4:]
    assert len(candidate_payload) == payload_length
    _, decoded_candidate = split_stream(candidate_payload, permit_zero_padding=True)
    assert decoded_candidate == changed_elf

    candidate = bytearray(original_kernel)
    candidate[payload_start:payload_end] = candidate_payload
    struct.pack_into("<I", candidate, len(candidate) - 4, zlib.crc32(candidate[:-4]) ^ 0xFFFFFFFF)
    candidate = bytes(candidate)
    assert len(candidate) == len(original_kernel)
    assert candidate[:payload_start] == original_kernel[:payload_start]
    assert candidate[payload_end:-4] == original_kernel[payload_end:-4]
    assert zlib.crc32(candidate) == 0xFFFFFFFF
    # All changes are confined to the fixed compressed payload slot and final CRC.
    assert all(payload_start <= i < payload_end or i >= len(candidate) - 4
               for i, (a, b) in enumerate(zip(original_kernel, candidate)) if a != b)

    report = {
        "createdAt": datetime.now(timezone.utc).isoformat(),
        "purpose": "Offline test candidate: disable only native built-in KSU initialization via Linux initcall blacklist",
        "runtimeValidated": False,
        "sourceKernelSha256": EXPECTED_KERNEL,
        "sourceElfSha256": EXPECTED_ELF,
        "candidateKernelSha256": sha(candidate),
        "candidateElfSha256": sha(changed_elf),
        "originalBytes": len(original_kernel),
        "candidateBytes": len(candidate),
        "protectedStart": protected_start,
        "payloadStart": payload_start,
        "payloadLength": payload_length,
        "payloadEnd": payload_end,
        "payloadUncompressedLengthTrailer": len(original_elf),
        "zeroPaddingBytes": padding_bytes,
        "compressionLibrary": library_name,
        "compressionLibraryVersion": library.LZ4_versionNumber(),
        "compressionLevel": 12,
        "recompressedBlockIndices": changed_block_indices,
        "originalBlockBytes": [len(b) for b in original_blocks],
        "candidateBlockBytes": [len(b) for b in candidate_blocks],
        "builtinCmdline": {"offset": hex(offset), "bufferBytes": 2048, "before": old_text.decode(), "after": new_text.decode(), "changedBytes": len(cmdline_changed_indices), "changedStart": hex(cmdline_changed_indices[0]), "changedEndExclusive": hex(cmdline_changed_indices[-1] + 1)},
        "removedAnalysisMetadata": {"start": hex(strip_start), "endExclusive": hex(strip_end), "regionBytes": strip_end - strip_start, "elfHeaderFieldsCleared": ["e_shoff", "e_shnum", "e_shstrndx"], "programHeadersPreserved": True, "programSegmentsPreservedExceptCmdline": True, "appendedRelocationsStart": hex(strip_end), "appendedRelocationsBytes": len(original_elf) - strip_end, "appendedRelocationsSha256": sha(original_elf[strip_end:]), "appendedRelocationsPreserved": True},
        "totalChangedElfBytes": len(changed_indices),
        "headerBytesIdentical": True,
        "loaderEfiTailBytesIdentical": True,
        "allNonPayloadNonCrcBytesIdentical": True,
        "independentPythonDecoderMatchesCandidateElf": True,
        "liblz4SafeDecoderMatchesAllChangedBlocks": True,
        "wholeFileZlibCrc32": hex(zlib.crc32(candidate)),
        "storedFinalCrc32": hex(u32(candidate, len(candidate) - 4)),
        "actualDecompressorProof": {"chunkReadFileOffset": "0xbbe740", "zeroTestFileOffset": "0xbbe76b", "zeroSuccessBranchFileOffset": "0xbbe76d", "successElfParseFileOffset": "0xbbe4f0", "lengthTrailerReadFileOffset": "0xbbe721"},
        "preserved": ["setup and PE headers", "compiled input/output lengths", "all boot loader/EFI code", "all decompressed code and drivers", "original ELF program segment layout", "entire appended relocation table", "original bzImage length"],
        "deployment": "Not performed; requires isolated instance boot, observed runtime cmdline and independent KSU absence checks",
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name in ("kernel", "kernel.elf", "report.json"):
        assert not (OUTPUT / name).exists(), f"refuse to overwrite existing candidate: {name}"
    (OUTPUT / "kernel").write_bytes(candidate)
    (OUTPUT / "kernel.elf").write_bytes(changed_elf)
    (OUTPUT / "report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    assert sha(args.kernel.read_bytes()) == EXPECTED_KERNEL
    assert sha(args.elf.read_bytes()) == EXPECTED_ELF
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
