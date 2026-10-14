"""Mask website advertisements while keeping ordinary punctuation and versions.

The bundled root-zone list is from https://data.iana.org/TLD/tlds-alpha-by-domain.txt.
It is local data: filtering never does DNS/network requests for a player's text.
"""
import ipaddress
import re
from pathlib import Path

_TLDS = frozenset(line.strip() for line in Path(__file__).with_name("tlds.txt").read_text().splitlines()
                 if line and not line.startswith("#")) | {"ONION", "I2P"}
_URL = re.compile(r"\b(?:(?:https?|hxxps?|ftp)\s*:\s*/\s*/|www\.)[^\s<>]+", re.I)
_SEP = r"(?:\s*[.。．｡]\s*|\s*\[\s*\.\s*\]\s*|\s*\(\s*\.\s*\)\s*|\s+(?:dot|точка)\s+)"
_LABEL = r"[\w](?:[\w-]{0,61}[\w])?"
_HOST = re.compile(r"(?<![\w-])(?:[\w.%+-]{1,64}@)?(?P<host>" + _LABEL + r"(?:" + _SEP + _LABEL + r"){1,8})"
                   r"(?::\d{1,5})?(?:/[^\s<>]*)?", re.I)
_SPLIT = re.compile(_SEP, re.I)
_IP = re.compile(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?::\d{1,5})?(?:/[^\s<>]*)?(?![\d.])")


def _mask(match: re.Match) -> str:
    text = match.group(0)
    end = len(text.rstrip(".,!?:;)]}"))
    return "***" + text[end:]


def clean(text: str) -> str:
    text = _URL.sub(_mask, text)

    def host(match: re.Match) -> str:
        name = match.group("host")
        parts = _SPLIT.split(name)
        # Avoid masking ordinary address abbreviations such as «г. Москва».
        if re.search(r"\s", name) and parts[0].lower() in {"г", "ул", "д", "стр", "корп", "кв"}:
            return match.group(0)
        try:
            tld = parts[-1].encode("idna").decode("ascii").upper()
        except UnicodeError:
            return match.group(0)
        return _mask(match) if tld in _TLDS else match.group(0)

    text = _HOST.sub(host, text)

    def ip(match: re.Match) -> str:
        address = match.group(0).split(":", 1)[0].split("/", 1)[0]
        try:
            ipaddress.IPv4Address(address)
        except ValueError:
            return match.group(0)
        return _mask(match)

    return _IP.sub(ip, text)
