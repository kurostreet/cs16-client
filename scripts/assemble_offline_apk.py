#!/usr/bin/env python3
"""Add local game data to a CI-built core APK, align ZIP entries, then sign and verify."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

def aligned_info(info, offset):
    info = copy.copy(info)
    info.extra = b""
    # A valid extra field supplies padding. Stored resources need 4-byte alignment.
    if info.compress_type == zipfile.ZIP_STORED:
        end = offset + 30 + len(info.filename.encode("utf-8"))
        padding = (-(end + 4)) % 4
        info.extra = struct.pack("<HH", 0xCAFE, padding) + b"\0" * padding
    return info

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--core", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--apksigner", required=True, type=Path)
    parser.add_argument("--keystore", required=True, type=Path, help="Public upstream debug keystore, for testing only")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    metadata = json.loads((args.assets / "bundled/game-data.json").read_text())
    pack = args.assets / "bundled/game-data.zip"
    with pack.open("rb") as stream:
        assert hashlib.file_digest(stream, "sha256").hexdigest() == metadata["sha256"]
    unsigned = args.output.with_suffix(".unsigned.apk")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.core) as source, zipfile.ZipFile(unsigned, "w") as target:
        for info in source.infolist():
            upper = info.filename.upper()
            if upper.startswith("META-INF/") and upper.endswith((".RSA", ".DSA", ".EC", ".SF", ".MF")):
                continue
            if info.filename in ("assets/bundled/game-data.zip", "assets/bundled/game-data.json"):
                continue
            info = aligned_info(info, target.fp.tell())
            with source.open(info.filename) as src, target.open(info, "w") as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
        for name in ("game-data.zip", "game-data.json"):
            path = args.assets / "bundled" / name
            info = zipfile.ZipInfo("assets/bundled/" + name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.file_size = path.stat().st_size
            info = aligned_info(info, target.fp.tell())
            with path.open("rb") as src, target.open(info, "w") as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
    with zipfile.ZipFile(unsigned) as archive, unsigned.open("rb") as raw:
        for info in archive.infolist():
            if info.compress_type == zipfile.ZIP_STORED:
                raw.seek(info.header_offset + 26)
                filename, extra = struct.unpack("<HH", raw.read(4))
                assert (info.header_offset + 30 + filename + extra) % 4 == 0, info.filename
    command = ["java", "-jar", str(args.apksigner)]
    subprocess.run(command + ["sign", "--ks", str(args.keystore), "--ks-pass", "pass:android",
                             "--key-pass", "pass:android", "--ks-key-alias", "androiddebugkey",
                             "--out", str(args.output), str(unsigned)], check=True)
    subprocess.run(command + ["verify", "--verbose", str(args.output)], check=True)
    unsigned.unlink()
    with args.output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    args.output.with_suffix(".apk.sha256").write_text(digest + "  " + args.output.name + "\n")
    print("Offline APK:", args.output, args.output.stat().st_size, digest)

if __name__ == "__main__":
    main()
