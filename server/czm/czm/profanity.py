"""Swear words in the chat become ***.

Words are found on a copy of the text where look-alike Latin letters and digits are
turned into Russian ones (x -> х, 3 -> з, 0 -> о...). Every character maps to exactly
one character, so the places found in the copy are the places in the original text.
The roots are tied to word starts where a root alone would hit normal words
("употребляю", "небо", "хлеб").
"""
import re
from . import links

_LOOKALIKE = str.maketrans({
    "a": "а", "b": "б", "c": "с", "e": "е", "k": "к", "m": "м", "o": "о", "p": "р",
    "t": "т", "x": "х", "y": "у", "u": "и", "h": "н", "n": "п",
    "3": "з", "0": "о", "6": "б", "@": "а", "$": "с", "ё": "е", "Ё": "е",
})

# prefixes the root "еб" takes in swearing ("заебал", "уебок", "выебан")
_EB_PREFIX = r"(?:за|по|на|от|у|вы|до|пере|про|раз|рас|с|съ|об|объ|под|подъ|в|въ|из|изъ|при|недо|ни|о|въ)?"

_RUSSIAN = [
    r"\b(?!пси|стра|застра)\w*?ху[йеяию]\w*",
    r"\w*пизд\w*",
    r"\b" + _EB_PREFIX + r"[её]б(?:а|у|л|н|и|о|ы|ё|ш|ч|ну)\w*",
    r"\w*долбо[её]б\w*",
    r"\bбля\w*",
    r"\bсу(?:к|ч)(?:а|и|е|у|ой|ам|ами|ах|ка|ки|ке|ку|кой|ий|ья|ьи|ара|ары)?\b",
    r"\bмуд(?:ак|ач|ил|озв|ень|ня)\w*",
    r"\w*пид(?:о|а|е)?р\w*",
    r"\bпедик\w*",
    r"\w*залуп\w*",
    r"\w*шлюх\w*",
    r"\w*г(?:а|о)ндон\w*",
    r"\bманд(?:а|ы|у|е|ой|ец|ят|авошк\w*)\b",
    r"\w*дроч\w*",
]

_ENGLISH = [
    r"\w*fuck\w*",
    r"\bshit\w*",
    r"\bbitch\w*",
    r"\bcunt\w*",
    r"\bdick(?:s|head\w*)?\b",
    r"\basshole\w*",
    r"\bnigg(?:er|a)\w*",
    r"\bfag(?:got)?s?\b",
    r"\bwhore\w*",
    r"\bslut\w*",
]

_RU_RE = re.compile("|".join(_RUSSIAN), re.IGNORECASE)
_EN_RE = re.compile("|".join(_ENGLISH), re.IGNORECASE)


def clean(text: str) -> str:
    """The text with swear words replaced by ***."""
    text = links.clean(text)
    spans = []
    lowered = text.lower()
    lookalike = lowered.translate(_LOOKALIKE)
    spans += [m.span() for m in _RU_RE.finditer(lookalike)]
    spans += [m.span() for m in _EN_RE.finditer(lowered)]
    if not spans:
        return text

    spans.sort()
    out, pos = [], 0
    for start, end in spans:
        if start < pos:  # overlaps the previous one
            continue
        out.append(text[pos:start])
        out.append("***")
        pos = end
    out.append(text[pos:])
    return "".join(out)
