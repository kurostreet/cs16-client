import os
import tempfile
import unittest
from unittest.mock import patch
from fastapi import FastAPI
from fastapi.testclient import TestClient
from czm import accounts, api, db, service, tab
from czm.config import get_settings


class TabTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        env = patch.dict(os.environ, {"CZM_DATA_DIR": temp.name}); env.start(); self.addCleanup(env.stop)
        get_settings.cache_clear(); self.addCleanup(get_settings.cache_clear); db.init()
        app = FastAPI(); app.include_router(api.router)
        self.client = TestClient(app); self.addCleanup(self.client.close)
        self.keys = ["legacy-avatar-owner-%d-123456" % i for i in range(3)]
        with db.db() as c:
            for i, key in enumerate(self.keys):
                service.hello(c, key, "device%d" % i, "Same nickname", str(i+1), "ru", "test")
            self.keys[0] = accounts.register(c, self.keys[0], "AvatarOwner", "Password123", "Password123", "ip")["token"]
        self.server = "203.0.113.10:27015"

    def bind(self, owner=0, userid=41, tag="", server=None):
        return self.client.post("/api/v1/tab/bind", headers={"X-CZM-Key": self.keys[owner]},
                                json={"server": server or self.server, "userid": userid, "tag": tag})

    def resolve(self, players, server=None):
        return self.client.post("/api/v1/tab/resolve", headers={"X-CZM-Key": self.keys[2]},
                                json={"server": server or self.server, "players": players})

    def test_account_and_guest_avatars_are_mutual_without_nickname_matching(self):
        a, b = self.bind().json(), self.bind(1, 42).json()
        result = self.resolve([{"userid": 41, "tag": a["tag"]}, {"userid": 42, "tag": b["tag"]}])
        self.assertEqual(result.status_code, 200, result.text)
        self.assertEqual([p["avatar"] for p in result.json()["players"]], ["1", "2"])
        self.assertEqual(result.headers["cache-control"], "no-store")
        self.assertEqual(len(a["tag"]), 32)
        self.assertNotIn(self.keys[0], result.text + str(a))

    def test_copying_tag_to_another_connection_or_server_fails(self):
        tag = self.bind().json()["tag"]
        self.assertEqual(self.resolve([{"userid": 42, "tag": tag}]).json()["players"], [])
        self.assertEqual(self.resolve([{"userid": 41, "tag": tag}], "203.0.113.10:27016").json()["players"], [])
        copied = self.bind(1, 42, tag).json()["tag"]
        self.assertNotEqual(copied, tag)
        self.assertEqual(self.resolve([{"userid": 41, "tag": tag}]).json()["players"][0]["avatar"], "1")

    def test_renewal_expiration_and_reconnect(self):
        tag = self.bind().json()["tag"]
        self.assertEqual(self.bind(tag=tag).json()["tag"], tag)
        new = self.bind(userid=77, tag=tag).json()["tag"]
        self.assertNotEqual(new, tag)
        self.assertEqual(self.resolve([{"userid": 41, "tag": tag}]).json()["players"], [])
        with patch("czm.tab.now", return_value=db.now() + tab.LIFETIME + 1):
            self.assertEqual(self.resolve([{"userid": 77, "tag": new}]).json()["players"], [])

    def test_logout_revokes_binding_and_profile_photo_revision_updates(self):
        tag = self.bind().json()["tag"]
        with db.db() as c:
            player = service.player_by_key(c, self.keys[0]); service.set_photo(c, player["id"])
        result = self.resolve([{"userid": 41, "tag": tag}]).json()["players"][0]
        self.assertEqual(result["avatar"], "photo:1")
        with db.db() as c:
            service.set_photo(c, player["id"]); accounts.logout(c, self.keys[0])
        self.assertEqual(self.resolve([{"userid": 41, "tag": tag}]).json()["players"], [])
        self.assertEqual(self.bind().status_code, 401)

    def test_guest_registration_revokes_legacy_binding(self):
        tag = self.bind(1, 42).json()["tag"]
        with db.db() as c: accounts.register(c, self.keys[1], "NewAccount", "Password123", "Password123", "ip2")
        self.assertEqual(self.resolve([{"userid": 42, "tag": tag}]).json()["players"], [])

    def test_input_limits_and_no_unauthenticated_binding(self):
        self.assertEqual(self.client.post("/api/v1/tab/bind", json={"server": self.server, "userid": 1}).status_code, 401)
        self.assertEqual(self.bind(server="http://127.0.0.1:8200").status_code, 400)
        self.assertEqual(self.bind(userid=0).status_code, 422)
        self.assertEqual(self.bind(tag="private-session-token").status_code, 422)
        self.assertEqual(self.resolve([{"userid": 1, "tag": "a"*32}]*33).status_code, 422)


if __name__ == "__main__": unittest.main()
