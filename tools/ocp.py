"""ocp.py — reference implementation of the Oscilla Control Protocol v1.

Pure Python, no dependencies, no I/O. The deck's C++ transport mirrors this.
Wire behaviour is protocol/OCP-SPEC.md.

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

# Mirrored from protocol/ocp.h; check_against_header() catches drift.

PROTO_VERSION = 1
BAUD_DEFAULT = 115200
MAX_LINE_LEN = 512
MAX_ARGV = 10

KW_BEGIN = "BEGIN"
KW_END = "END"
MARK_EVT = "[EVT]"
MARK_ERR = "[ERR]"
MARK_HELLO = "[HELLO]"
REPLY_PONG = "pong"

HEADER_PATH = Path(__file__).resolve().parent.parent / "protocol" / "ocp.h"

_MARKER_RE = re.compile(r"^\[[A-Z0-9_]+\]$")
_BARE_KEY_RE = re.compile(r"^[A-Za-z0-9_.]+$")
# Commas are legal unquoted: caps=wifi24,wifi5 and set_channels 1,6,11 rely on
# it. A CSV row always quotes every field, so there is no ambiguity.
_BARE_VALUE_RE = re.compile(r"^[A-Za-z0-9_.:+,\-]*$")


# --- §6 Text, quoting, escaping --------------------------------------------


def encode_field(raw: bytes | str) -> str:
    """Encode a text field: quoted, escaped, printable ASCII (OCP-SPEC §6).

    Reversible byte-for-byte by decode_field. Every attacker-controlled string
    must go through here or a newline in an SSID desynchronises the reader.
    """
    if isinstance(raw, str):
        raw = raw.encode("utf-8", "surrogateescape")
    out = ['"']
    for b in raw:
        if b == 0x5C:      # backslash
            out.append("\\\\")
        elif b == 0x22:    # double quote
            out.append('\\"')
        elif 0x20 <= b <= 0x7E:
            out.append(chr(b))
        else:
            out.append(f"\\x{b:02x}")
    out.append('"')
    return "".join(out)


def decode_field(text: str) -> bytes:
    """Decode one wire field to its original bytes, quoted or not."""
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        text = text[1:-1]
    out = bytearray()
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c != "\\":
            out.extend(c.encode("utf-8", "surrogateescape"))
            i += 1
            continue
        if i + 1 >= n:
            raise OcpFramingError("trailing backslash in field")
        esc = text[i + 1]
        if esc == "x":
            if i + 3 >= n:
                raise OcpFramingError("truncated \\xHH escape")
            try:
                out.append(int(text[i + 2 : i + 4], 16))
            except ValueError:
                raise OcpFramingError(f"bad \\xHH escape: {text[i:i+4]!r}") from None
            i += 4
        elif esc in ('"', "\\"):
            out.extend(esc.encode())
            i += 2
        else:
            raise OcpFramingError(f"unknown escape: \\{esc}")
    return bytes(out)


def encode_value(raw: bytes | str) -> str:
    """Encode a k=v value, quoting only when the spec requires it."""
    text = raw.decode("utf-8", "surrogateescape") if isinstance(raw, bytes) else raw
    if text and _BARE_VALUE_RE.match(text):
        return text
    return encode_field(raw)


class OcpFramingError(ValueError):
    """Unparseable line. The line is discarded, never the stream."""


def tokenize(text: str) -> list[str]:
    """Split a line into tokens, keeping a quoted run intact.

    Unterminated quote raises, so the line is discarded rather than consuming
    the next one (OCP-SPEC §6).
    """
    tokens: list[str] = []
    cur: list[str] = []
    in_quote = False
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if in_quote:
            cur.append(c)
            if c == "\\" and i + 1 < n:
                cur.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_quote = False
            i += 1
            continue
        if c == '"':
            in_quote = True
            cur.append(c)
            i += 1
            continue
        if c in " \t":
            if cur:
                tokens.append("".join(cur))
                cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    if in_quote:
        raise OcpFramingError("unterminated quoted field")
    if cur:
        tokens.append("".join(cur))
    return tokens


def parse_kv(tokens: list[str]) -> dict[str, bytes]:
    """Parse k=v tokens into a mapping; other tokens are ignored."""
    out: dict[str, bytes] = {}
    for tok in tokens:
        if "=" not in tok:
            continue
        key, _, val = tok.partition("=")
        if not _BARE_KEY_RE.match(key):
            continue
        out[key] = decode_field(val) if val.startswith('"') else val.encode()
    return out


def split_csv_row(text: str) -> list[bytes]:
    """Split a [SCAN]-family CSV row into decoded fields."""
    fields: list[bytes] = []
    cur: list[str] = []
    in_quote = False
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if in_quote:
            cur.append(c)
            if c == "\\" and i + 1 < n:
                cur.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_quote = False
            i += 1
            continue
        if c == '"':
            in_quote = True
            cur.append(c)
        elif c == ",":
            fields.append(decode_field("".join(cur).strip()))
            cur = []
        else:
            cur.append(c)
        i += 1
    if in_quote:
        raise OcpFramingError("unterminated quoted field in CSV row")
    fields.append(decode_field("".join(cur).strip()))
    return fields


# --- Parsed items -----------------------------------------------------------


@dataclass
class Frame:
    """A complete [TAG] frame, compact or block."""

    tag: str
    kv: dict[str, bytes] = field(default_factory=dict)
    rows: list[str] = field(default_factory=list)
    compact: bool = False

    def csv_rows(self) -> list[list[bytes]]:
        return [split_csv_row(r) for r in self.rows]

    def row_kvs(self) -> list[dict[str, bytes]]:
        return [parse_kv(tokenize(r)) for r in self.rows]

    def get(self, key: str, default: str | None = None) -> str | None:
        val = self.kv.get(key)
        return default if val is None else val.decode("utf-8", "replace")


@dataclass
class Event:
    """An unsolicited [EVT] line. Lossy by design."""

    kind: str
    kv: dict[str, bytes] = field(default_factory=dict)


@dataclass
class Error:
    """An [ERR] line. `code` is machine-readable; `msg` never is."""

    code: str
    msg: str = ""
    kv: dict[str, bytes] = field(default_factory=dict)


@dataclass
class Pong:
    """The one unbracketed reply."""


@dataclass
class Noise:
    """Non-protocol line: boot chatter, log spam, malformed input.

    Surfaced so tools can show it, but it never advances frame state.
    """

    text: str
    reason: str = "not a marker"


Item = Frame | Event | Error | Pong | Noise


# --- Parser -----------------------------------------------------------------


class OcpParser:
    """Line-oriented OCP reader. Never raises on input; bad lines yield Noise.

    An open block frame is held until its END. [EVT]/[ERR] arriving inside one
    are emitted without disturbing it (OCP-SPEC §5.1).
    """

    def __init__(self, max_line_len: int = MAX_LINE_LEN) -> None:
        self.max_line_len = max_line_len
        self._open: Frame | None = None
        self._buf = bytearray()
        self._overlong = False

    # -- byte-level ---------------------------------------------------------

    def feed_bytes(self, data: bytes) -> Iterator[Item]:
        """Feed raw serial bytes; handles line splitting and the length cap."""
        for byte in data:
            if byte == 0x0A:
                line = bytes(self._buf)
                overlong = self._overlong
                self._buf.clear()
                self._overlong = False
                if overlong:
                    yield Noise(
                        line.decode("utf-8", "replace"),
                        f"line exceeded {self.max_line_len} bytes",
                    )
                else:
                    yield from self.feed_line(line.decode("utf-8", "replace"))
                continue
            if self._overlong:
                continue  # discard to end of line
            self._buf.append(byte)
            if len(self._buf) > self.max_line_len:
                self._overlong = True
                del self._buf[self.max_line_len :]

    # -- line-level ---------------------------------------------------------

    def feed_line(self, line: str) -> Iterator[Item]:
        line = line.rstrip("\r\n")
        stripped = line.strip()
        if not stripped:
            return

        try:
            tokens = tokenize(stripped)
        except OcpFramingError as exc:
            yield Noise(line, str(exc))
            return
        if not tokens:
            return

        tag = tokens[0]

        if tag == REPLY_PONG and len(tokens) == 1:
            yield Pong()
            return

        if not _MARKER_RE.match(tag):
            yield Noise(line)
            return

        rest = tokens[1:]
        raw_rest = stripped[len(tag) :].strip()

        # [EVT] and [ERR] are bare lines; they never open or close a frame.
        if tag == MARK_EVT:
            kv = parse_kv(rest)
            yield Event(kv.get("kind", b"").decode("utf-8", "replace"), kv)
            return
        if tag == MARK_ERR:
            kv = parse_kv(rest)
            yield Error(
                kv.get("code", b"").decode("utf-8", "replace"),
                kv.get("msg", b"").decode("utf-8", "replace"),
                kv,
            )
            return

        # [HELLO] outranks parser state: a probe that reset mid-frame leaves
        # the open frame dead, and swallowing it strands the deck (§4.1).
        if tag == MARK_HELLO and self._open is not None and self._open.tag != MARK_HELLO:
            abandoned = self._open
            self._open = None
            yield Noise(
                f"{abandoned.tag} BEGIN",
                "frame abandoned: probe announced a reset mid-frame",
            )

        # Inside an open frame, only that frame's own tag is meaningful.
        if self._open is not None and tag != self._open.tag:
            yield Noise(line, f"tag {tag} inside open {self._open.tag} frame")
            return

        if rest and rest[0] == KW_BEGIN:
            if self._open is not None:
                # A second BEGIN means the first was never closed.
                abandoned = self._open
                yield Noise(
                    f"{abandoned.tag} BEGIN", "frame re-opened without END"
                )
            self._open = Frame(tag=tag, kv=parse_kv(rest[1:]))
            return

        if rest and rest[-1] == KW_END:
            if self._open is not None:
                frame, self._open = self._open, None
                yield frame
                return
            # Compact frame: [TAG] k=v ... END on one line.
            yield Frame(tag=tag, kv=parse_kv(rest[:-1]), compact=True)
            return

        if self._open is not None:
            self._open.rows.append(raw_rest)
            return

        yield Noise(line, "row outside any frame")

    # -- lifecycle ----------------------------------------------------------

    def abandon_open_frame(self) -> Frame | None:
        """Drop a half-read frame on timeout or reset, and stay usable.

        Returns what was abandoned so a caller can log it.
        """
        frame, self._open = self._open, None
        self._buf.clear()
        self._overlong = False
        return frame

    @property
    def frame_open(self) -> bool:
        return self._open is not None


# --- Command encoding -------------------------------------------------------


def encode_command(verb: str, *args: str | bytes) -> bytes:
    """Encode a command line, quoting arguments that need it."""
    if len(args) + 1 > MAX_ARGV:
        raise ValueError(f"{verb}: {len(args)} args exceeds MAX_ARGV-1")
    parts = [verb, *(encode_value(a) for a in args)]
    line = " ".join(parts)
    if len(line) > MAX_LINE_LEN:
        raise ValueError(f"{verb}: command line exceeds MAX_LINE_LEN")
    return (line + "\n").encode("ascii")


# --- Header drift check -----------------------------------------------------


def read_header_defines(path: Path | None = None) -> dict[str, str]:
    """Extract `#define NAME value` pairs from protocol/ocp.h."""
    src = (path or HEADER_PATH).read_text(encoding="utf-8")
    out: dict[str, str] = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(.+?)\s*(?:/\*.*)?$", src, re.M):
        name, val = m.group(1), m.group(2).strip()
        if val.startswith('"') and val.endswith('"'):
            val = val[1:-1]
        out[name] = val
    return out


def header_verbs(path: Path | None = None) -> list[str]:
    """Every verb in OCP_VERB_TABLE — the whole command surface."""
    src = (path or HEADER_PATH).read_text(encoding="utf-8")
    table = src.split("#define OCP_VERB_TABLE(X)", 1)
    if len(table) < 2:
        raise RuntimeError("OCP_VERB_TABLE not found in ocp.h")
    defines = read_header_defines(path)
    verbs = []
    for m in re.finditer(r"^\s*X\((\w+),\s*(\w+),", table[1], re.M):
        macro = m.group(2)
        if macro not in defines:
            raise RuntimeError(f"verb macro {macro} has no #define")
        verbs.append(defines[macro])
        if re.search(r"^\s*#define OCP_VERB_TABLE", table[1][: m.start()], re.M):
            break
    return verbs


def check_against_header(path: Path | None = None) -> list[str]:
    """Return drift complaints; empty means this file matches ocp.h."""
    d = read_header_defines(path)
    problems = []
    expect = {
        "OCP_PROTO_VERSION": str(PROTO_VERSION),
        "OCP_BAUD_DEFAULT": str(BAUD_DEFAULT),
        "OCP_MAX_LINE_LEN": str(MAX_LINE_LEN),
        "OCP_MAX_ARGV": str(MAX_ARGV),
        "OCP_KW_BEGIN": KW_BEGIN,
        "OCP_KW_END": KW_END,
        "OCP_MARK_EVT": MARK_EVT,
        "OCP_MARK_ERR": MARK_ERR,
        "OCP_MARK_HELLO": MARK_HELLO,
        "OCP_REPLY_PONG": REPLY_PONG,
    }
    for name, want in expect.items():
        got = d.get(name)
        if got is None:
            problems.append(f"{name} missing from ocp.h")
        elif got != want:
            problems.append(f"{name}: ocp.h has {got!r}, ocp.py has {want!r}")
    return problems
