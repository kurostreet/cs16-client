"""SQLite storage. One connection per call, WAL mode; the service is small."""
import sqlite3
import threading
import time
from contextlib import contextmanager
from typing import Iterator

from .config import get_settings

SCHEMA = """
CREATE TABLE IF NOT EXISTS players (
    id INTEGER PRIMARY KEY,
    key_hash TEXT NOT NULL UNIQUE,
    device_hash TEXT NOT NULL DEFAULT '',
    nick TEXT NOT NULL DEFAULT 'Player',
    avatar TEXT NOT NULL DEFAULT '',      -- '', '1'..'10' or 'photo'
    avatar_rev INTEGER NOT NULL DEFAULT 0,
    lang TEXT NOT NULL DEFAULT '',
    version TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL,
    seen INTEGER NOT NULL,
    last_message INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS players_device ON players(device_hash);

CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY,
    room TEXT NOT NULL,
    player INTEGER NOT NULL,
    text TEXT NOT NULL,
    created INTEGER NOT NULL,
    deleted INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS messages_room ON messages(room, id);

CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY,
    reporter INTEGER NOT NULL,
    target INTEGER NOT NULL,
    message INTEGER,                      -- NULL: the avatar
    created INTEGER NOT NULL,
    status TEXT NOT NULL DEFAULT 'open',  -- open, banned, deleted, dismissed
    resolved INTEGER
);
CREATE INDEX IF NOT EXISTS reports_target ON reports(target);

CREATE TABLE IF NOT EXISTS bans (
    id INTEGER PRIMARY KEY,
    player INTEGER NOT NULL,
    device_hash TEXT NOT NULL DEFAULT '',
    until INTEGER,                        -- NULL: forever
    reason TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL,
    lifted INTEGER
);
CREATE INDEX IF NOT EXISTS bans_player ON bans(player);
CREATE INDEX IF NOT EXISTS bans_device ON bans(device_hash);

CREATE TABLE IF NOT EXISTS news (
    id INTEGER PRIMARY KEY,
    text TEXT NOT NULL,
    created INTEGER NOT NULL,
    deleted INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS releases (
    id INTEGER PRIMARY KEY,
    version TEXT NOT NULL,
    code INTEGER NOT NULL,
    file TEXT NOT NULL,
    size INTEGER NOT NULL,
    sha256 TEXT NOT NULL,
    notes TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL,
    published INTEGER
);
"""

_lock = threading.Lock()


def now() -> int:
    return int(time.time())


def connect() -> sqlite3.Connection:
    path = get_settings().db_path
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(path, timeout=10, isolation_level=None)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn


@contextmanager
def db() -> Iterator[sqlite3.Connection]:
    """A connection with one write transaction at a time."""
    with _lock:
        conn = connect()
        try:
            conn.execute("BEGIN IMMEDIATE")
            yield conn
            conn.execute("COMMIT")
        except BaseException:
            conn.execute("ROLLBACK")
            raise
        finally:
            conn.close()


def init() -> None:
    with _lock:
        conn = connect()
        try:
            conn.executescript(SCHEMA)
        finally:
            conn.close()


def cleanup(conn: sqlite3.Connection, keep_days: int, room_history: int, rooms: list[str]) -> None:
    """Old messages go away: older than keep_days, or beyond the history of a room."""
    conn.execute("DELETE FROM messages WHERE created < ?", (now() - keep_days * 86400,))
    for room in rooms:
        conn.execute(
            "DELETE FROM messages WHERE room = ? AND id NOT IN "
            "(SELECT id FROM messages WHERE room = ? ORDER BY id DESC LIMIT ?)",
            (room, room, room_history * 3),
        )
