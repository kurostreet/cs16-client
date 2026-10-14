"""Streaming APK deltas. ZIP payloads are reused; signing blocks remain literal bytes.

Wire format: CZMPAT01, big-endian uint32 JSON length, UTF-8 recipe, literal data.
Operations [0, offset, length] read the installed APK; [1, offset, length] read
the literal block. The output is the original signed APK, without re-signing.
"""
import hashlib
import json
import struct
import zipfile
from pathlib import Path

MAGIC = b"CZMPAT01"
MAX_HEADER = 2 << 20
MAX_OPS = 50000
MAX_APK = 2 << 30
CHUNK = 1 << 20


class PatchError(ValueError):
    pass


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(CHUNK), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _range(stream, offset: int, length: int):
    stream.seek(offset)
    while length:
        chunk = stream.read(min(CHUNK, length))
        if not chunk:
            raise PatchError("truncated input")
        yield chunk
        length -= len(chunk)


def _payload(raw, info: zipfile.ZipInfo, size: int) -> int:
    raw.seek(info.header_offset)
    header = raw.read(30)
    if len(header) != 30 or header[:4] != b"PK\x03\x04":
        raise PatchError("bad ZIP local header")
    name, extra = struct.unpack_from("<HH", header, 26)
    offset = info.header_offset + 30 + name + extra
    if offset < 0 or offset + info.compress_size > size:
        raise PatchError("ZIP payload outside APK")
    return offset


def _equal(a, b, a_offset: int, b_offset: int, length: int) -> bool:
    a.seek(a_offset)
    b.seek(b_offset)
    while length:
        count = min(CHUNK, length)
        left, right = a.read(count), b.read(count)
        if len(left) != count or len(right) != count or left != right:
            return False
        length -= count
    return True


def make_patch(base: Path, target: Path, patch: Path, *, base_sha256: str = "", target_sha256: str = "") -> dict | None:
    """Create and verify a patch; return None if downloading it would save no bytes.

    Only a few streaming buffers and ZIP/recipe metadata are kept in memory.
    Hashes supplied by the release database are checked before publication.
    """
    base_hash, target_hash = sha256_of(base), sha256_of(target)
    if base_sha256 and base_hash != base_sha256 or target_sha256 and target_hash != target_sha256:
        raise PatchError("APK hash differs from release metadata")
    base_size, target_size = base.stat().st_size, target.stat().st_size
    if not 0 < base_size <= MAX_APK or not 0 < target_size <= MAX_APK:
        raise PatchError("APK size outside limits")
    ops, literals = [], []
    cursor = data_size = reused = 0
    with zipfile.ZipFile(base) as old, zipfile.ZipFile(target) as new, base.open("rb") as a, target.open("rb") as b:
        if len(set(old.namelist())) != len(old.infolist()) or len(set(new.namelist())) != len(new.infolist()):
            raise PatchError("duplicate ZIP entry")
        for info in sorted(new.infolist(), key=lambda item: item.header_offset):
            if not info.compress_size:
                continue
            try:
                prev = old.getinfo(info.filename)
            except KeyError:
                continue
            if (prev.CRC, prev.file_size, prev.compress_size, prev.compress_type, prev.flag_bits) != \
                    (info.CRC, info.file_size, info.compress_size, info.compress_type, info.flag_bits):
                continue
            start = _payload(b, info, target_size)
            source = _payload(a, prev, base_size)
            if start < cursor:
                raise PatchError("overlapping ZIP payloads")
            if not _equal(a, b, source, start, info.compress_size):
                continue
            if start > cursor:
                length = start - cursor
                ops.append([1, data_size, length])
                literals.append((cursor, length))
                data_size += length
            ops.append([0, source, info.compress_size])
            reused += info.compress_size
            cursor = start + info.compress_size
    if cursor < target_size:
        ops.append([1, data_size, target_size - cursor])
        literals.append((cursor, target_size - cursor))
        data_size += target_size - cursor
    recipe = {"format": 1, "base_sha256": base_hash, "sha256": target_hash,
              "base_size": base_size, "size": target_size, "data_size": data_size, "ops": ops}
    header = json.dumps(recipe, separators=(",", ":")).encode("utf-8")
    if len(header) > MAX_HEADER or len(ops) > MAX_OPS:
        raise PatchError("recipe outside limits")
    if 12 + len(header) + data_size >= target_size:
        return None
    partial = patch.with_name(patch.name + ".tmp")
    try:
        with partial.open("wb") as out, target.open("rb") as raw:
            out.write(MAGIC + struct.pack(">I", len(header)) + header)
            for offset, length in literals:
                for chunk in _range(raw, offset, length):
                    out.write(chunk)
        verify_patch(base, partial, target_hash, target_size)
        digest, size = sha256_of(partial), partial.stat().st_size
        partial.replace(patch)
        patch.chmod(0o644)
        return {"base_sha256": base_hash, "sha256": digest, "size": size, "file": patch.name, "reused": reused}
    finally:
        partial.unlink(missing_ok=True)


def verify_patch(base: Path, patch: Path, expected_sha256: str, expected_size: int, output: Path | None = None) -> None:
    """Reconstruct into a hash (or a file) before advertising a patch to players."""
    with patch.open("rb") as data, base.open("rb") as original:
        if data.read(8) != MAGIC:
            raise PatchError("bad patch magic")
        size_bytes = data.read(4)
        if len(size_bytes) != 4:
            raise PatchError("truncated header")
        length = struct.unpack(">I", size_bytes)[0]
        if not 0 < length <= MAX_HEADER:
            raise PatchError("header outside limits")
        try:
            recipe = json.loads(data.read(length))
        except (ValueError, UnicodeError) as e:
            raise PatchError("bad recipe") from e
        data_start = 12 + length
        if (recipe.get("format") != 1 or recipe.get("base_sha256") != sha256_of(base)
                or recipe.get("sha256") != expected_sha256 or recipe.get("size") != expected_size
                or recipe.get("base_size") != base.stat().st_size or not 0 < expected_size <= MAX_APK):
            raise PatchError("recipe does not match APKs")
        data_size, ops = recipe.get("data_size"), recipe.get("ops")
        if (type(data_size) is not int or data_size < 0 or data_start + data_size != patch.stat().st_size
                or not isinstance(ops, list) or not 0 < len(ops) <= MAX_OPS):
            raise PatchError("bad literal size or operation count")
        digest = hashlib.sha256()
        produced = used = 0
        out = output.open("wb") if output is not None else None
        try:
            for op in ops:
                if not isinstance(op, list) or len(op) != 3 or any(type(n) is not int for n in op):
                    raise PatchError("bad operation")
                kind, offset, count = op
                if kind not in (0, 1) or offset < 0 or count <= 0 or count > expected_size - produced:
                    raise PatchError("operation outside output")
                limit = base.stat().st_size if kind == 0 else data_size
                if offset > limit or count > limit - offset or kind == 1 and offset != used:
                    raise PatchError("operation outside source")
                for chunk in _range(original if kind == 0 else data, offset if kind == 0 else data_start + offset, count):
                    digest.update(chunk)
                    if out is not None:
                        out.write(chunk)
                produced += count
                if kind == 1:
                    used += count
            if produced != expected_size or used != data_size or digest.hexdigest() != expected_sha256:
                raise PatchError("reconstructed APK hash differs")
        finally:
            if out is not None:
                out.close()
