"""Settings of the Counter Zombie Mobile service, read from /etc/czm/env (CZM_*)."""
from functools import lru_cache
from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="CZM_", extra="ignore")

    data_dir: Path = Path("/var/lib/czm")       # database, avatars, APK files, server list
    public_url: str = "https://kontrafps.ru"   # where players reach the service
    pepper: str = "change-me"                  # mixed into stored key and device hashes

    # Telegram bot of the owner: complaints, bans, news, releases
    bot_token: str = ""
    bot_api_url: str = ""                      # relay for the Bot API, Telegram is blocked from the server
    bot_owner_ids: str = ""                    # comma-separated Telegram user IDs
    bot_polling_timeout_seconds: int = 25

    # world server list from Steam (IGameServersService/GetServerList, appid 10)
    steam_key: str = ""
    steam_refresh_seconds: int = 300

    # chat rules
    rooms: str = "global,ru,en"
    message_max_chars: int = 200
    message_interval_seconds: float = 3.0
    room_history: int = 100
    keep_days: int = 7

    @property
    def owner_ids(self) -> set[int]:
        ids = set()
        for part in self.bot_owner_ids.replace(" ", "").split(","):
            if part.lstrip("-").isdigit():
                ids.add(int(part))
        return ids

    @property
    def room_list(self) -> list[str]:
        return [r.strip() for r in self.rooms.split(",") if r.strip()]

    @property
    def db_path(self) -> Path:
        return self.data_dir / "czm.sqlite3"

    @property
    def avatar_dir(self) -> Path:
        return self.data_dir / "avatars"

    @property
    def apk_dir(self) -> Path:
        return self.data_dir / "apk"

    @property
    def serverlist_file(self) -> Path:
        return self.data_dir / "server-list" / "v1" / "servers" / "cstrike"


@lru_cache
def get_settings() -> Settings:
    return Settings()
