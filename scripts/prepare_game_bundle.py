#!/usr/bin/env python3
"""Normalize an owner's supplied valve/cstrike archive for offline first-run installation."""
import argparse
from pathlib import Path
import zipfile
from create_resource_pack import make_pack

REQUIRED = ("valve/halflife.wad", "cstrike/cstrike.wad",
            "cstrike/maps/de_dust2.bsp", "cstrike/models/player/terror/terror.mdl")
NATIVE_EXTENSIONS = {".dll", ".so", ".exe", ".dex", ".jar", ".apk"}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output_directory", type=Path, help="Asset root containing bundled/")
    args = parser.parse_args()
    with zipfile.ZipFile(args.source) as source:
        entries = {}
        for item in source.infolist():
            if item.is_dir():
                continue
            parts = item.filename.split("/")
            if "xash" in parts:
                parts = parts[parts.index("xash") + 1:]
            if not parts or parts[0] not in ("valve", "cstrike"):
                continue
            name = "/".join(parts)
            if Path(name).suffix.lower() in NATIVE_EXTENSIONS:
                continue
            if name in entries:
                raise ValueError("Duplicate game file: " + name)
            entries[name] = lambda name=item.filename: source.read(name)
        for required in REQUIRED:
            if required not in entries:
                raise ValueError("Missing game resource: " + required)
        # Native Android game libraries are compiled from source and packaged under lib/.
        # There is no reason to ship the original PC binaries from the data archive.
        make_pack(args.output_directory / "bundled/game-data.zip", entries, REQUIRED)

if __name__ == "__main__":
    main()
