#!/usr/bin/env python3
"""Apply Counter Zombie Mobile changes on top of the pinned Xash3D FWGS submodule.

The submodule itself stays pinned to upstream; our changes live in patches/xash3d-fwgs/.
Running this more than once is safe: already applied patches are skipped.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "3rdparty/xash3d-fwgs"
ENGINE_PATCHES = ROOT / "patches/xash3d-fwgs"


def apply_engine_patches():
    patches = sorted(ENGINE_PATCHES.glob("*.patch"))
    if not patches:
        return
    # Reverse-checking each patch separately fails once a later patch changes its
    # context. Reconstruct the complete sequence away from the user's checkout.
    # Accept only an exact known prefix, never reset or overwrite unknown edits.
    paths = set()
    for patch in patches:
        for path in re.findall(r"^(?:--- a/|\+\+\+ b/)(.+)$", patch.read_text(), re.M):
            if Path(path).is_absolute() or ".." in Path(path).parts:
                raise ValueError("Unsafe engine patch path")
            paths.add(path)

    def contents(folder):
        return {name: (folder / name).read_bytes() if (folder / name).is_file() else None for name in paths}

    subprocess.run(["git", "rev-parse", "HEAD"], cwd=ENGINE, check=True, stdout=subprocess.DEVNULL)
    actual = contents(ENGINE)
    prefix = None
    with tempfile.TemporaryDirectory(prefix="czm-engine-check-") as temporary:
        tree = Path(temporary)
        for name in paths:
            result = subprocess.run(["git", "show", "HEAD:" + name], cwd=ENGINE,
                                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            if result.returncode == 0:
                target = tree / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(result.stdout)
        if contents(tree) == actual:
            prefix = 0
        for index, patch in enumerate(patches, 1):
            subprocess.run(["git", "apply", "--whitespace=nowarn", str(patch.resolve())], cwd=tree, check=True)
            if contents(tree) == actual:
                prefix = index
    if prefix is None:
        raise RuntimeError("Engine files contain changes outside the known patch sequence; nothing was overwritten")
    for index, patch in enumerate(patches):
        if index < prefix:
            print("Engine patch already applied:", patch.name, flush=True)
        else:
            subprocess.run(["git", "apply", "--whitespace=nowarn", str(patch.resolve())], cwd=ENGINE, check=True)
            print("Engine patch applied:", patch.name, flush=True)


if __name__ == "__main__":
    apply_engine_patches()
