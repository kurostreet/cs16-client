import concurrent.futures
import io
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from fastapi import FastAPI
from fastapi.testclient import TestClient
from PIL import Image
from czm import api, db, mappics, profanity, service
from czm.config import get_settings


def png(color):
    out = io.BytesIO()
    Image.new("RGB", (640, 360), color).save(out, "PNG")
    return out.getvalue()


class LinkTests(unittest.TestCase):
    def test_sites_and_common_obfuscation(self):
        for value in ["https://market-pay.ru", "MARKET-PAY.RU", "t.me/czm", "сайт.рф", "site[.]com",
                      "site (.) ru", "site точка ru", "site dot com", "www.example.invalid", "1.2.3.4:27015", "a@site.com"]:
            with self.subTest(value=value):
                self.assertEqual(profanity.clean("Заходи " + value + "!"), "Заходи ***!")

    def test_normal_text_versions_and_punctuation(self):
        for value in ["Привет.Как дела?", "г. Москва", "Версия 1.35", "FPS 120, пинг 37 мс", "1.2.3.999", "Играем вместе!"]:
            with self.subTest(value=value):
                self.assertEqual(profanity.clean(value), value)


class MapPictureTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        env = patch.dict(os.environ, {"CZM_DATA_DIR": temp.name})
        env.start()
        self.addCleanup(env.stop)
        get_settings.cache_clear()
        self.addCleanup(get_settings.cache_clear)
        db.init()
        app = FastAPI()
        app.include_router(api.router)
        self.client = TestClient(app)
        self.addCleanup(self.client.close)
        self.key = "mappic-test-private-key-123456789"
        with db.db() as conn:
            service.hello(conn, self.key, "", "Test", "", "ru", "test")
        self.headers = {"X-CZM-Key": self.key}

    def test_upload_needs_profile_valid_image_and_first_wins(self):
        url = "/api/v1/mappic/zm_new_map"
        image = png((90, 120, 160))
        self.assertEqual(self.client.post(url, content=image).status_code, 401)
        self.assertEqual(self.client.post(url, content=b"invalid", headers=self.headers).status_code, 400)
        self.assertEqual(self.client.post(url, content=png("black"), headers=self.headers).status_code, 422)
        self.assertEqual(self.client.post(url, content=image, headers=self.headers).status_code, 201)
        path = get_settings().mappic_dir / "zm_new_map.png"
        first = path.read_bytes()
        with Image.open(path) as picture:
            self.assertEqual(picture.size, (300, 170))
        self.assertEqual(self.client.post(url, content=png("white"), headers=self.headers).status_code, 409)
        self.assertEqual(path.read_bytes(), first)
        self.assertEqual(self.client.post("/api/v1/mappic/a..b", content=image, headers=self.headers).status_code, 400)

    def test_publish_has_one_atomic_winner(self):
        folder = get_settings().mappic_dir
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda i: mappics.publish(folder, "zm_concurrent", bytes([i]) * 1000), range(8)))
        self.assertEqual(sum(results), 1)
        data = (folder / "zm_concurrent.png").read_bytes()
        self.assertEqual(len(data), 1000)
        self.assertEqual(len(set(data)), 1)
        self.assertEqual(len(list(folder.iterdir())), 1)

    def test_old_chat_history_is_masked_without_destroying_original(self):
        with db.db() as conn:
            player = service.player_by_key(conn, self.key)
            conn.execute("INSERT INTO messages(player,room,text,created) VALUES (?,?,?,?)",
                         (player["id"], "global", "https://market-pay.ru", 1))
            self.assertEqual(service.messages(conn, "global", 0)[0]["t"], "***")
            self.assertEqual(conn.execute("SELECT text FROM messages").fetchone()[0], "https://market-pay.ru")


if __name__ == "__main__":
    unittest.main()
