"""Shared map screenshots. Only the first valid picture is published, atomically."""
import io
import os
import re
import tempfile
from pathlib import Path

from PIL import Image, ImageOps

_NAME = re.compile(r"[a-z0-9][a-z0-9_.-]{0,62}\Z", re.ASCII)
MAX_BYTES = 300 * 1024


def map_name(value: str) -> str:
    value = value.lower()
    if not _NAME.fullmatch(value) or ".." in value:
        raise ValueError("map")
    return value


def prepare(data: bytes) -> bytes:
    if not data or len(data) > MAX_BYTES:
        raise ValueError("size")
    with Image.open(io.BytesIO(data)) as image:
        if image.format not in ("PNG", "JPEG", "WEBP") or image.width * image.height > 4096 * 4096:
            raise ValueError("image")
        rgb = ImageOps.fit(image.convert("RGB"), (300, 170), method=Image.Resampling.LANCZOS)
    histogram = ImageOps.grayscale(rgb).histogram()
    pixels = 300 * 170
    if sum(i * n for i, n in enumerate(histogram)) / pixels < 12 or sum(histogram[24:]) / pixels < .05:
        raise ValueError("dark")
    out = io.BytesIO()
    rgb.save(out, "PNG", optimize=True)
    return out.getvalue()


def publish(folder: Path, name: str, png: bytes) -> bool:
    name = map_name(name)
    folder.mkdir(parents=True, exist_ok=True)
    # A hard link publishes the completed inode without replacing another winner.
    fd, temporary = tempfile.mkstemp(prefix=".mappic-", dir=folder)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(png)
            stream.flush()
            os.fsync(stream.fileno())
            os.fchmod(stream.fileno(), 0o644)
        try:
            os.link(temporary, folder / f"{name}.png")
            return True
        except FileExistsError:
            return False
    finally:
        Path(temporary).unlink(missing_ok=True)
