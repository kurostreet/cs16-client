"""Public avatar bindings for one game connection. Private credentials stay on HTTPS."""
import ipaddress
import secrets

from . import accounts, service
from .db import now

LIFETIME = 180


def server_address(value):
    try:
        host, port = value.rsplit(":", 1)
        ip = ipaddress.ip_address(host.strip("[]"))
        port = int(port)
        if not 1 <= port <= 65535:
            raise ValueError
        return f"[{ip}]:{port}" if ip.version == 6 else f"{ip}:{port}"
    except ValueError:
        raise accounts.AuthError("server") from None


def bind(conn, key, server, userid, tag=""):
    player = service.player_by_key(conn, key)
    if player is None:
        raise accounts.AuthError("session", 401)
    server = server_address(server)
    owner = service.hash_secret(key)
    t = now()
    conn.execute("DELETE FROM tab_bindings WHERE expires<=?", (t,))
    row = conn.execute("SELECT * FROM tab_bindings WHERE tag=? AND owner=? AND server=? AND userid=?",
                       (tag, owner, server, userid)).fetchone()
    if row:
        conn.execute("UPDATE tab_bindings SET expires=? WHERE tag=?", (t + LIFETIME, tag))
    else:
        # One active connection per device/session. A copied public tag cannot be
        # renewed by another owner or rebound to another server-assigned userid.
        conn.execute("DELETE FROM tab_bindings WHERE owner=?", (owner,))
        tag = secrets.token_hex(16)
        conn.execute("INSERT INTO tab_bindings(tag,owner,player,guest,server,userid,expires) VALUES(?,?,?,?,?,?,?)",
                     (tag, owner, player["id"], not key.startswith(accounts.SESSION_PREFIX), server, userid, t + LIFETIME))
    return {"tag": tag, "ttl": LIFETIME}


def resolve(conn, key, server, players):
    if service.player_by_key(conn, key) is None:
        raise accounts.AuthError("session", 401)
    server = server_address(server)
    t = now()
    result = []
    for item in players:
        row = conn.execute(
            "SELECT b.*,p.avatar,p.avatar_rev FROM tab_bindings b JOIN players p ON p.id=b.player "
            "WHERE b.tag=? AND b.server=? AND b.userid=? AND b.expires>? AND "
            "((b.guest=1 AND b.owner=p.key_hash AND NOT EXISTS(SELECT 1 FROM accounts a WHERE a.player=p.id)) OR "
            "(b.guest=0 AND EXISTS(SELECT 1 FROM account_sessions s WHERE s.player=p.id AND s.token_hash=b.owner "
            "AND s.expires>? AND s.seen>?)))",
            (item["tag"], server, item["userid"], t, t, t - accounts.SESSION_IDLE),
        ).fetchone()
        if row:
            result.append({"tag": row["tag"], "userid": row["userid"], "id": row["player"],
                           "avatar": service.avatar_ref(row), "ttl": row["expires"] - t})
    return {"players": result}
