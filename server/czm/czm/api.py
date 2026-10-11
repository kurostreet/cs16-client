"""HTTP API for the game. JSON, the player's secret key comes in the X-CZM-Key header."""
import asyncio
import io
import logging
import time
from collections import defaultdict, deque
from typing import Any, Callable

from fastapi import APIRouter, Header, HTTPException, Request
from fastapi.responses import JSONResponse
from PIL import Image
from pydantic import BaseModel, Field

from . import service
from .config import get_settings
from .db import db

log = logging.getLogger("czm.api")
router = APIRouter(prefix="/api/v1")

# the bot gets complaints through this hook (set by main.py)
notify_report: Callable[[dict], Any] | None = None


async def in_db(fn: Callable, *args: Any) -> Any:
    def run():
        with db() as conn:
            return fn(conn, *args)
    return await asyncio.to_thread(run)


class _Limiter:
    """At most `count` calls per `seconds` for one key (an IP address)."""

    def __init__(self, count: int, seconds: float):
        self.count, self.seconds = count, seconds
        self.calls: dict[str, deque] = defaultdict(deque)

    def check(self, key: str) -> None:
        t = time.monotonic()
        q = self.calls[key]
        while q and q[0] < t - self.seconds:
            q.popleft()
        if len(q) >= self.count:
            raise HTTPException(429, {"error": "slow", "wait": int(self.seconds)})
        q.append(t)
        if len(self.calls) > 20000:
            self.calls.clear()


_hello_limit = _Limiter(20, 60)
_read_limit = _Limiter(120, 60)
_upload_limit = _Limiter(6, 600)


def client_ip(request: Request) -> str:
    # Caddy in front sets X-Forwarded-For
    forwarded = request.headers.get("x-forwarded-for", "")
    return forwarded.split(",")[0].strip() or (request.client.host if request.client else "")


async def current_player(key: str | None):
    player = await in_db(service.player_by_key, key or "")
    if player is None:
        raise HTTPException(401, {"error": "hello"})
    return player


def check_room(room: str) -> str:
    if room not in get_settings().room_list:
        raise HTTPException(404, {"error": "room"})
    return room


class HelloIn(BaseModel):
    key: str = Field(min_length=16, max_length=128)
    device: str = Field(default="", max_length=128)
    nick: str = Field(default="", max_length=64)
    avatar: str = Field(default="", max_length=16)
    lang: str = Field(default="", max_length=8)
    version: str = Field(default="", max_length=32)
    code: int = 0


@router.post("/hello")
async def hello(body: HelloIn, request: Request):
    _hello_limit.check(client_ip(request))

    def run(conn):
        player = service.hello(conn, body.key, body.device, body.nick, body.avatar, body.lang, body.version)
        release = service.latest_release(conn)
        return {
            "id": player["id"],
            "avatar": service.avatar_ref(player),
            "ban": service.active_ban(conn, player),
            "news": service.last_news_id(conn),
            "update": release if release and release["code"] > body.code else None,
            "rooms": get_settings().room_list,
            "max_chars": get_settings().message_max_chars,
        }

    return await in_db(run)


@router.get("/chat/{room}")
async def chat(room: str, request: Request, after: int = 0, x_czm_key: str | None = Header(default=None)):
    _read_limit.check(client_ip(request))
    check_room(room)
    player = await current_player(x_czm_key)

    def run(conn):
        return {
            "messages": service.messages(conn, room, max(0, after)),
            "deleted": service.deleted_since(conn, room, after),
            "ban": service.active_ban(conn, player),
        }

    return await in_db(run)


class MessageIn(BaseModel):
    text: str = Field(max_length=1000)


@router.post("/chat/{room}")
async def post(room: str, body: MessageIn, x_czm_key: str | None = Header(default=None)):
    check_room(room)
    player = await current_player(x_czm_key)
    try:
        return await in_db(service.post_message, player, room, body.text)
    except service.ChatError as e:
        return JSONResponse({"error": e.code, **e.extra}, status_code=e.status)


class ReportIn(BaseModel):
    message: int | None = None
    player: int | None = None


@router.post("/report")
async def report(body: ReportIn, x_czm_key: str | None = Header(default=None)):
    reporter = await current_player(x_czm_key)
    try:
        info = await in_db(service.report, reporter, body.message, body.player)
    except service.ChatError as e:
        return JSONResponse({"error": e.code, **e.extra}, status_code=e.status)
    if info and notify_report:
        asyncio.create_task(notify_report(info))
    return {"ok": True}


@router.post("/avatar")
async def avatar(request: Request, x_czm_key: str | None = Header(default=None)):
    """The player's own photo: a PNG or JPEG up to 512 KB, stored as a 192 px PNG."""
    _upload_limit.check(client_ip(request))
    player = await current_player(x_czm_key)
    data = await request.body()
    if not data or len(data) > 512 * 1024:
        raise HTTPException(413, {"error": "size"})

    def convert() -> bytes:
        image = Image.open(io.BytesIO(data))
        if image.format not in ("PNG", "JPEG", "WEBP") or image.width * image.height > 4096 * 4096:
            raise ValueError("format")
        image = image.convert("RGBA").resize((192, 192), Image.LANCZOS)
        out = io.BytesIO()
        image.save(out, "PNG", optimize=True)
        return out.getvalue()

    try:
        png = await asyncio.to_thread(convert)
    except Exception:
        raise HTTPException(400, {"error": "image"})

    ban = await in_db(service.active_ban, player)
    if ban:
        return JSONResponse({"error": "banned", "ban": ban}, status_code=403)

    folder = get_settings().avatar_dir
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"{player['id']}.png"
    tmp = path.with_suffix(".tmp")
    tmp.write_bytes(png)
    tmp.replace(path)
    rev = await in_db(service.set_photo, player["id"])
    return {"avatar": f"photo:{rev}"}


@router.get("/news")
async def news(request: Request, after: int = 0):
    _read_limit.check(client_ip(request))
    return {"news": await in_db(service.news, max(0, after))}


@router.get("/update")
async def update(request: Request, code: int = 0):
    _read_limit.check(client_ip(request))
    release = await in_db(service.latest_release)
    return {"update": release if release and release["code"] > code else None}
