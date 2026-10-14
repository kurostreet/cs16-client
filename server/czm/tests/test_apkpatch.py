"""Run: PYTHONPATH=server/czm python -m unittest discover -s server/czm/tests."""
import hashlib
import json
import os
import random
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from czm.apkpatch import MAGIC, MAX_HEADER, PatchError, make_patch, sha256_of, verify_patch


def apk(path, entries, signature=b"", comment=b""):
    with zipfile.ZipFile(path, "w") as archive:
        for name, content, kind in entries:
            archive.writestr(zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0)), content, compress_type=kind)
        archive.comment = comment
    if signature:
        data = bytearray(path.read_bytes())
        eocd = data.rfind(b"PK\x05\x06")
        central = struct.unpack_from("<I", data, eocd + 16)[0]
        struct.pack_into("<I", data, eocd + 16, central + len(signature))
        path.write_bytes(data[:central] + signature + data[central:])


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base, self.target, self.patch = [self.root / name for name in ("base.apk", "target.apk", "patch.bin")]
        noise = random.Random(42).randbytes(300000)
        stable = [("assets/game-data.zip", noise, zipfile.ZIP_STORED), ("lib/stable.so", noise[:100000], zipfile.ZIP_DEFLATED)]
        apk(self.base, stable + [("lib/menu.so", b"old menu", zipfile.ZIP_DEFLATED)], b"old signing block" * 20)
        apk(self.target, [("new.txt", b"new entry", zipfile.ZIP_DEFLATED)] + list(reversed(stable)) +
            [("lib/menu.so", b"new menu", zipfile.ZIP_DEFLATED)], b"new signing block" * 31, b"target comment")
        self.sha, self.size = sha256_of(self.target), self.target.stat().st_size
        self.info = make_patch(self.base, self.target, self.patch)

    def recipe(self):
        data = self.patch.read_bytes()
        length = struct.unpack_from(">I", data, 8)[0]
        return json.loads(data[12:12 + length]), data[12 + length:]

    def write_recipe(self, recipe, data):
        header = json.dumps(recipe, separators=(",", ":")).encode()
        self.patch.write_bytes(MAGIC + struct.pack(">I", len(header)) + header + data)

    def test_signed_layout_reordered_payloads_and_comment_are_exact(self):
        out = self.root / "restored.apk"
        verify_patch(self.base, self.patch, self.sha, self.size, out)
        self.assertEqual(out.read_bytes(), self.target.read_bytes())
        self.assertGreater(self.info["reused"], 390000)
        self.assertLess(self.info["size"], self.size // 10)
        self.assertEqual(self.info["sha256"], sha256_of(self.patch))

    def test_wrong_base_and_target_hash(self):
        self.base.write_bytes(self.base.read_bytes() + b"different")
        with self.assertRaises(PatchError):
            verify_patch(self.base, self.patch, self.sha, self.size)
        with self.assertRaises(PatchError):
            make_patch(self.base, self.target, self.patch, base_sha256="0" * 64)

    def test_corrupt_or_truncated_payload_is_rejected(self):
        data = self.patch.read_bytes()
        for broken in (data[:-1], data + b"extra", data[:-1] + bytes([data[-1] ^ 1]), b"wrong" + data[5:], data[:10]):
            with self.subTest(length=len(broken)):
                self.patch.write_bytes(broken)
                with self.assertRaises(PatchError):
                    verify_patch(self.base, self.patch, self.sha, self.size)

    def test_recipe_source_and_output_bounds(self):
        recipe, data = self.recipe()
        for op in ([0, -1, 1], [0, 2**63 - 1, 1], [0, 0, 2**63 - 1], [1, 1, 1], [2, 0, 1], [0, 0, 0], [True, 0, 1]):
            with self.subTest(op=op):
                changed = {**recipe, "ops": [op]}
                self.write_recipe(changed, data)
                with self.assertRaises(PatchError):
                    verify_patch(self.base, self.patch, self.sha, self.size)
        self.write_recipe([], data)
        with self.assertRaises(PatchError):
            verify_patch(self.base, self.patch, self.sha, self.size)
        self.patch.write_bytes(MAGIC + struct.pack(">I", MAX_HEADER + 1))
        with self.assertRaises(PatchError):
            verify_patch(self.base, self.patch, self.sha, self.size)

    def test_no_savings_does_not_create_a_patch(self):
        other = self.root / "different.apk"
        apk(other, [("unrelated", os.urandom(100000), zipfile.ZIP_STORED)])
        dest = self.root / "unused.patch"
        self.assertIsNone(make_patch(self.base, other, dest))
        self.assertFalse(dest.exists())

    def test_matching_crc_does_not_replace_a_byte_comparison(self):
        with zipfile.ZipFile(self.target) as archive:
            info = archive.getinfo("assets/game-data.zip")
        data = bytearray(self.target.read_bytes())
        name, extra = struct.unpack_from("<HH", data, info.header_offset + 26)
        payload = info.header_offset + 30 + name + extra
        data[payload + 5] ^= 1  # central directory metadata remains identical
        self.target.write_bytes(data)
        info = make_patch(self.base, self.target, self.patch)
        self.assertIsNotNone(info)
        out = self.root / "restored.apk"
        verify_patch(self.base, self.patch, sha256_of(self.target), len(data), out)
        self.assertEqual(out.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
