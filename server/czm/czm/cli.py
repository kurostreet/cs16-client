"""Server commands: python -m czm.cli add-release <apk> <version> <code>

add-release moves an uploaded APK into the download folder and remembers it as the
newest build. Players see it only after the owner publishes it in the bot (/release).
"""
import hashlib
import shutil
import sys
from pathlib import Path

from . import db
from .config import get_settings


def add_release(apk: Path, version: str, code: int) -> None:
    settings = get_settings()
    db.init()
    digest = hashlib.sha256()
    with apk.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    name = f"CounterZombieMobile-{version}.apk"
    target = settings.apk_dir / name
    settings.apk_dir.mkdir(parents=True, exist_ok=True)
    shutil.move(str(apk), target)
    target.chmod(0o644)
    with db.db() as conn:
        conn.execute(
            "INSERT INTO releases (version, code, file, size, sha256, created) VALUES (?, ?, ?, ?, ?, ?)",
            (version, code, name, target.stat().st_size, digest.hexdigest(), db.now()),
        )
    print(f"release {version} ({code}) ready: {target.stat().st_size // 1024 // 1024} MB, publish it in the bot with /release")


def main(argv: list[str]) -> int:
    if len(argv) == 4 and argv[0] == "add-release" and argv[3].isdigit():
        add_release(Path(argv[1]), argv[2], int(argv[3]))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
