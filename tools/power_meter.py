"""power_meter.py — host side of bench-power-meter's USB stream.

Splits the one stream into the meter's `#PWR` records and the probe's OCP
items, and reduces windows to per-state figures. Pure; no serial, no
hardware. Line format: bench-power-meter/README.md. Method:
docs/hardware/power-budget.md.

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterator

from ocp import MAX_LINE_LEN, OcpFramingError, OcpParser, parse_kv, tokenize

PWR_TAG = "#PWR"

WIN_INT_FIELDS = (
    "t0_us", "t1_us", "n", "i_ua", "i_min_ua", "i_max_ua", "vbus_mv",
    "vbus_min_mv", "sag", "clip", "cuts", "txdrop", "i2c_err", "lost",
)


@dataclass
class Meter:
    """One `#PWR <kind> k=v …` line."""

    kind: str
    kv: dict[str, str] = field(default_factory=dict)

    def int(self, key: str) -> int:
        return int(self.kv[key])


def parse_pwr(line: str) -> Meter | None:
    """A meter record, or None if this line is not one (or is malformed)."""
    if not line.startswith(PWR_TAG + " "):
        return None
    try:
        tokens = tokenize(line)
        kv = parse_kv(tokens[2:])
    except (OcpFramingError, ValueError):
        return None
    if len(tokens) < 2:
        return None
    rec = Meter(tokens[1], {k: v.decode("utf-8", "replace") for k, v in kv.items()})
    if rec.kind == "win":
        try:
            for f in WIN_INT_FIELDS:
                rec.int(f)
        except (KeyError, ValueError):
            return None
    return rec


class Demux:
    """Bytes in; Meter records and OCP items out, in stream order."""

    def __init__(self) -> None:
        self.ocp = OcpParser()
        self._buf = bytearray()
        self._overlong = False

    def feed(self, data: bytes) -> Iterator[object]:
        for byte in data:
            if byte != 0x0A:
                if not self._overlong:
                    self._buf.append(byte)
                    if len(self._buf) > MAX_LINE_LEN:
                        self._overlong = True
                continue
            line = bytes(self._buf).decode("latin-1").rstrip("\r")
            overlong, self._overlong = self._overlong, False
            self._buf.clear()
            if overlong:
                continue
            rec = parse_pwr(line)
            if rec is not None:
                yield rec
            else:
                yield from self.ocp.feed_line(line)


@dataclass
class StateFigures:
    label: str
    t0_us: int
    t1_us: int
    windows: int = 0
    mean_ua: float | None = None
    min_ua: int | None = None
    max_ua: int | None = None
    vbus_min_mv: int | None = None
    problems: list[str] = field(default_factory=list)

    @property
    def valid(self) -> bool:
        return self.windows > 0 and not self.problems


def summarize(windows: list[Meter], marks: list[Meter], end_us: int,
              settle_us: int, probe_events: list[Meter] = (),
              errors: dict[str, list[str]] | None = None) -> list[StateFigures]:
    """Slice windows by mark; skip each state's first `settle_us`.

    Only windows wholly inside [mark + settle, next mark) count. The mean is
    weighted by window duration, since windows can stretch (README "Output").
    """
    out = []
    bounds = [m.int("t_us") for m in marks] + [end_us]
    counters_before = None
    for i, mark in enumerate(marks):
        st = StateFigures(mark.kv.get("label", "?"), bounds[i], bounds[i + 1])
        lo = bounds[i] + settle_us
        inside = [w for w in windows if w.int("t0_us") >= lo and w.int("t1_us") <= st.t1_us]
        span = [w for w in windows if w.int("t1_us") > st.t0_us and w.int("t0_us") < st.t1_us]
        if inside:
            dur = [w.int("t1_us") - w.int("t0_us") for w in inside]
            st.windows = len(inside)
            st.mean_ua = sum(w.int("i_ua") * d for w, d in zip(inside, dur)) / sum(dur)
            st.min_ua = min(w.int("i_min_ua") for w in inside)
            st.max_ua = max(w.int("i_max_ua") for w in inside)
            vb = [w.int("vbus_min_mv") for w in inside if w.int("vbus_min_mv") >= 0]
            st.vbus_min_mv = min(vb) if vb else None
        else:
            st.problems.append("no windows after settle")
        if any(w.int("sag") for w in span):
            st.problems.append("supply sagged")
        if any(w.int("clip") for w in span):
            st.problems.append("shunt ADC clipped")
        if any(w.int("n") == 0 for w in inside):
            st.problems.append("window with no samples")
        # Cumulative counters: any rise across this state's span is a problem.
        if span:
            first = counters_before or span[0]
            for key, what in (("cuts", "probe line cut"), ("i2c_err", "i2c errors"),
                              ("lost", "meter lines lost"), ("txdrop", "commands dropped")):
                if span[-1].int(key) > first.int(key):
                    st.problems.append(what)
            counters_before = span[-1]
        for ev in probe_events:
            if st.t0_us <= ev.int("t_us") < st.t1_us and ev.kv.get("state") == "down":
                st.problems.append("probe lost power")
        for msg in (errors or {}).get(st.label, []):
            st.problems.append(msg)
        out.append(st)
    return out
