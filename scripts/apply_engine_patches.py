#!/usr/bin/env python3
"""Apply Counter Zombie Mobile changes on top of the pinned Xash3D FWGS submodule.

The submodule itself stays pinned to upstream; our changes live in patches/xash3d-fwgs/.
Running this more than once is safe: already applied patches are skipped.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "3rdparty/xash3d-fwgs"
ENGINE_PATCHES = ROOT / "patches/xash3d-fwgs"


def apply_engine_patches():
    for patch in sorted(ENGINE_PATCHES.glob("*.patch")):
        applied = subprocess.run(["git", "apply", "--reverse", "--check", str(patch)], cwd=ENGINE,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        if applied:
            print("Engine patch already applied:", patch.name, flush=True)
            continue
        subprocess.run(["git", "apply", "--whitespace=nowarn", str(patch)], cwd=ENGINE, check=True)
        print("Engine patch applied:", patch.name, flush=True)


if __name__ == "__main__":
    apply_engine_patches()
