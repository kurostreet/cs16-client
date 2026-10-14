"""Exercise the actual Java updater against HTTP and server-generated binary patches.

CZM_JAVA_CLASSES and CZM_JSON_JAR point at the compiled Java test and org.json.
No Android device, application installation or production endpoint is involved.
"""
import hashlib
import http.server
import json
import os
import random
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "server/czm"))
from czm.apkpatch import MAGIC, make_patch, sha256_of


class UpdateTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.base, self.target, patch = [self.root / name for name in ("base.apk", "target.apk", "patch.bin")]
        data = random.Random(17).randbytes(400000)
        for path, menu in ((self.base, b"old menu"), (self.target, b"changed menu")):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("assets/game-data.zip", data)
                archive.writestr("lib/menu.so", menu)
        self.base_sha, self.sha = sha256_of(self.base), sha256_of(self.target)
        self.size = self.target.stat().st_size
        self.patch_info = make_patch(self.base, self.target, patch)
        self.payload = patch.read_bytes()
        self.hits, self.mode = [], "normal"
        case = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                case.hits.append((self.path, self.headers.get("Range")))
                if self.path == "/patch" and case.mode == "missing":
                    self.send_error(404)
                    return
                data = case.payload if self.path == "/patch" else case.target.read_bytes()
                if self.path == "/patch" and case.mode == "corrupt":
                    data = data[:-1] + bytes([data[-1] ^ 1])
                offset = 0
                requested = self.headers.get("Range")
                if (requested and case.mode != "ignore-range") or (case.mode == "bad-range" and self.path == "/patch"):
                    offset = int(requested[6:-1]) if requested else 0
                    self.send_response(206)
                    start = offset + 1 if case.mode == "bad-range" and self.path == "/patch" else offset
                    self.send_header("Content-Range", f"bytes {start}-{len(data)-1}/{len(data)}")
                else:
                    self.send_response(200)
                self.send_header("Content-Length", str(len(data) - offset))
                self.end_headers()
                if self.path == "/patch" and case.mode == "interrupted" and not requested:
                    data = data[:len(data) // 2]
                try:
                    self.wfile.write(data[offset:])
                except (BrokenPipeError, ConnectionResetError):
                    pass

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.url = f"http://127.0.0.1:{server.server_port}"
        self.dir = self.root / "update"
        self.dir.mkdir()

    def run_update(self, *, cancel="none", base_sha=None, patch_sha=None, sha=None, success=True):
        classes = os.environ["CZM_JAVA_CLASSES"]
        json_jar = os.environ["CZM_JSON_JAR"]
        command = ["java", "-cp", classes + os.pathsep + json_jar, "ApkUpdateTest", str(self.dir), str(self.base),
                   self.url + "/apk", sha or self.sha, str(self.size), self.url + "/patch",
                   patch_sha or self.patch_info["sha256"], str(len(self.payload)), base_sha or self.base_sha, cancel]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0 if success else 2, result.stderr + result.stdout)
        if cancel != "none":
            self.assertIn("CANCELLED", result.stdout)
            self.assertNotIn("READY", result.stdout)
        elif success:
            self.assertIn("READY", result.stdout)
            self.assertEqual((self.dir / f"czm-{self.sha}.apk").read_bytes(), self.target.read_bytes())
        self.assertFalse(list(self.dir.glob("*.building")))
        return result

    def partial(self):
        (self.dir / f"czm-{self.sha}.patch").write_bytes(self.payload[:31])

    def test_patch_reconstructs_byte_exact_apk_without_full_download(self):
        self.run_update()
        self.assertEqual([path for path, _ in self.hits], ["/patch"])
        self.assertEqual(sha256_of(self.base), self.base_sha)

    def test_patch_download_resumes_after_interruption(self):
        self.mode = "interrupted"
        self.run_update()
        self.assertTrue(any(header for _, header in self.hits))
        self.assertFalse(any(path == "/apk" for path, _ in self.hits))

    def test_range_and_server_ignoring_range(self):
        for mode in ("normal", "ignore-range"):
            with self.subTest(mode=mode):
                for file in self.dir.iterdir():
                    file.unlink()
                self.hits.clear()
                self.mode = mode
                self.partial()
                self.run_update()
                self.assertEqual(self.hits[0], ("/patch", "bytes=31-"))
                self.assertFalse(any(path == "/apk" for path, _ in self.hits))

    def test_wrong_base_skips_patch(self):
        self.base.write_bytes(self.base.read_bytes() + b"other signature")
        result = self.run_update()
        self.assertEqual([path for path, _ in self.hits], ["/apk"])
        self.assertIn(f"download 0 {self.size}", result.stdout)

    def test_corruption_and_missing_patch_fall_back_to_full(self):
        for mode in ("corrupt", "missing", "bad-range"):
            with self.subTest(mode=mode):
                for file in self.dir.iterdir():
                    file.unlink()
                self.hits.clear()
                self.mode = mode
                if mode == "bad-range":
                    self.partial()
                self.run_update()
                self.assertTrue(any(path == "/apk" for path, _ in self.hits))

    def test_malformed_recipe_with_correct_patch_hash_falls_back(self):
        n = struct.unpack_from(">I", self.payload, 8)[0]
        recipe = json.loads(self.payload[12:12+n])
        recipe["ops"][0] = [0, 2**63 - 1, 1]
        header = json.dumps(recipe, separators=(",", ":")).encode()
        self.payload = MAGIC + struct.pack(">I", len(header)) + header + self.payload[12+n:]
        self.run_update(patch_sha=hashlib.sha256(self.payload).hexdigest())
        self.assertTrue(any(path == "/apk" for path, _ in self.hits))

    def test_cancel_does_not_start_full_download_and_can_resume(self):
        self.run_update(cancel="download")
        self.assertFalse(any(path == "/apk" for path, _ in self.hits))
        self.run_update()

    def test_cancel_during_base_check_does_not_download(self):
        self.run_update(cancel="verify")
        self.assertFalse(self.hits)

    def test_full_download_checksum_is_required(self):
        self.run_update(base_sha="1" * 64, sha="0" * 64, success=False)
        self.assertFalse(list(self.dir.glob("*.apk")))

    def test_cached_corrupt_apk_is_downloaded_again(self):
        (self.dir / f"czm-{self.sha}.apk").write_bytes(b"x" * self.size)
        self.run_update(base_sha="1" * 64)
        self.assertTrue(any(path == "/apk" for path, _ in self.hits))


if __name__ == "__main__":
    unittest.main()
