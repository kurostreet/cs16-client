"""World list of Counter-Strike 1.6 servers for the game's server list.

Valve's old UDP master is gone; Steam knows the servers through its Web API
(IGameServersService/GetServerList, appid 10), which needs the owner's key.
The list is written in the format of the engine's static masters ("gs ip:port"),
the game reads it from http://kontrafps.ru/server-list/v1/servers/cstrike.
"""
import asyncio
import logging
import time

import httpx

from .config import get_settings

log = logging.getLogger("czm.steam")
URL = "https://api.steampowered.com/IGameServersService/GetServerList/v1/"


async def fetch_once(client: httpx.AsyncClient) -> int:
    settings = get_settings()
    response = await client.get(URL, params={"key": settings.steam_key, "filter": r"\appid\10", "limit": 20000})
    response.raise_for_status()
    servers = response.json().get("response", {}).get("servers", [])

    lines = [f"# Counter-Strike 1.6 servers from Steam, {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime())} UTC", "# Format: <ip|gs> <address>", ""]
    seen = set()
    for s in servers:
        addr = s.get("addr", "")
        if s.get("gamedir") != "cstrike" or not addr or addr in seen:
            continue
        seen.add(addr)
        lines.append(f"gs {addr}")

    path = settings.serverlist_file
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text("\n".join(lines) + "\n")
    tmp.replace(path)
    return len(seen)


async def run() -> None:
    settings = get_settings()
    if not settings.steam_key:
        log.info("no Steam key, the world server list is off")
        return
    async with httpx.AsyncClient(timeout=60) as client:
        while True:
            try:
                count = await fetch_once(client)
                log.info("world server list: %d servers", count)
            except asyncio.CancelledError:
                raise
            except Exception as e:
                # the key itself never goes to the log
                log.warning("Steam server list failed: %s", type(e).__name__)
            await asyncio.sleep(settings.steam_refresh_seconds)
