# Oscilla Control Protocol (OCP) — v1

> Normative wire specification. Mirrors [`DESIGN.md`](../DESIGN.md) §5; where this document is more specific, it is because an implementation needed an answer. Literals (verbs, markers, caps, error codes, limits) are defined once in [`ocp.h`](ocp.h) — **this document never restates a literal that the header owns**; it says what the bytes mean.
>
> **proto = 1.** Additions (new verbs, new `[EVT] kind=`, new `k=v` keys) do not bump it. Framing or handshake changes do.

---

## 1. Transport

| Property | Value |
|---|---|
| Link | UART, 3.3 V, no flow control, no level shifter |
| Rate | `OCP_BAUD_DEFAULT` 8N1, fixed at boot |
| Deck pins | S3 GPIO2 TX → C5 GPIO12 RX; S3 GPIO1 RX ← C5 GPIO11 TX (Rev D §3) |
| Direction | Commands deck → probe, one line each. Frames and events probe → deck |
| Terminator | `\n` sent; the probe also accepts `\r\n` |
| Encoding | 7-bit ASCII on the wire (§6 defines how other bytes are carried) |

Both ends cap line length at `OCP_MAX_LINE_LEN`, queue depth at `OCP_EVENT_QUEUE_DEPTH`, and rows per block frame at `OCP_MAX_FRAME_ROWS`, and **drop rather than block** (Rev D §10). A frame that would exceed the row cap is abandoned whole, never delivered truncated, since a short result is indistinguishable from a complete one. A producer with more rows than the cap must page. A line longer than the cap is truncated at the cap, the remainder is discarded up to the next `\n`, and the truncated line is not treated as a frame.

Rate is fixed at boot so a plain terminal always works. A higher rate may be negotiated after the handshake; nothing in v1 does.

### 1.1 Boot noise is normal traffic

C5 GPIO11 is UART0's default pin, so ROM and bootloader text appears on this wire every time the probe resets (Rev D §3, DESIGN §4.2). This is not an error condition and is not suppressible. The deck's parser therefore:

- ignores any line that is not a known marker or an expected row of the frame it is currently reading;
- never lets unparseable input advance a frame's state;
- treats an unsolicited `[HELLO]` as "the probe rebooted, my state is stale".

**A parser that can wedge on boot text is a defect, not a tuning problem.**

---

## 2. Commands

```
verb [arg1 [arg2 ...]]\n
```

- Tokens are separated by one or more spaces (0x20) or tabs (0x09); leading and trailing spaces and tabs are ignored. **No other byte is whitespace.** Trailing CR and LF bytes end the line. Parsers work on bytes, never on decoded text.
- An empty line is ignored, not an error.
- Verbs are case-sensitive and lowercase.
- Indices are **1-based** (`inspect_network 1` is the first row of the last scan).
- At most `OCP_MAX_ARGV` tokens including the verb; excess tokens are `badarg`.
- Arguments containing spaces are double-quoted and unescaped per §6.

Each verb's arity and required capability class live in the `OCP_VERB_TABLE` X-macro in [`ocp.h`](ocp.h) — the probe's dispatch table is generated from it, so the table *is* the command surface.

### 2.1 Ordering and concurrency

One command is in flight at a time. The probe processes commands in order and does not pipeline. A long-running mode (`start_sniffer`, `lora_listen`, …) **returns its reply frame immediately** and continues to emit `[EVT]` lines; it does not hold the command channel. `stop` is always accepted, including while a mode runs (DESIGN §6.4: the dispatch task outranks engine tasks, so `stop` always lands).

---

## 3. Frames

Every machine-readable response is one of three shapes. The tag is always the **first whitespace-delimited token** on the line, which is what makes the line recogniser cheap and resync-safe.

### 3.1 Block frame

For anything with rows:

```
[SCAN] BEGIN n=3
[SCAN] "1","HomeNet","AA:BB:CC:DD:EE:01","6","WPA2","-52","2.4"
[SCAN] "2","","AA:BB:CC:DD:EE:02","36","WPA3","-71","5"
[SCAN] "3","Caf\xc3\xa9 Wi-Fi","AA:BB:CC:DD:EE:03","11","OPEN","-80","2.4"
[SCAN] END
```

- The `BEGIN` line may carry `k=v` context (`n=` is a hint, not a contract — trust the rows you receive, not the count).
- Every row repeats the tag. A row is distinguished from `BEGIN`/`END` by its second token.
- `END` is **unconditional**: it is emitted even for zero rows, even when the operation failed midway. A parser therefore always terminates.

### 3.2 Compact frame

For a frame that is all context and no rows, `BEGIN` and `END` collapse onto one line, which is how `[HELLO]` appears in DESIGN §5.3:

```
[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,wifi5,ble,ieee802154,lora_rx END
```

A line is a compact frame iff its second token is **not** `BEGIN`, its final token is `END` (outside quotes), and **at least one token lies between them**. A bare `[TAG] END` is only ever a block terminator; with no frame open it is noise — typically the late `END` of a frame already abandoned by a timeout or a reset, which must not surface as an empty result. Compact and block forms are otherwise equivalent; a parser must accept either for any tag, because a producer may grow rows later without bumping `proto`.

### 3.3 Bare line

`[EVT]` and `[ERR]` are single lines with no `BEGIN`/`END`:

```
[EVT] kind=sniff pkts=1423 ch=6
[ERR] code=busy owner=ble msg="radio in use"
```

### 3.4 The one non-marker reply

`ping` is answered with the bare word `pong` — no brackets. It is the only unbracketed machine-readable reply, kept so a human on a serial terminal gets a readable answer. A deck parser must match it explicitly (`OCP_REPLY_PONG`); it is not discoverable by the marker rule.

### 3.5 Forward compatibility

**Unknown markers and unknown `k=v` keys are ignored, never fatal.** This is what lets the two firmwares be flashed independently. Concretely, a deck must not:

- treat an unrecognised `[TAG]` as an error (skip lines until a tag it knows);
- treat an unrecognised `kind=` on `[EVT]` as an error (drop the event);
- treat an extra `k=v` on a frame it understands as an error (ignore the key);
- require keys to appear in any particular order.

---

## 4. Handshake

The deck opens every session with `hello` and must not send any other verb until it has a `[HELLO]`.

```
> hello
[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,wifi5,ble,ieee802154,lora_rx END
```

| Key | Meaning |
|---|---|
| `proto` | integer protocol version; the deck refuses a probe whose value it does not understand |
| `fw` | firmware identity (`OCP_FW_NAME_PROBE`) |
| `ver` | firmware semver, informational |
| `caps` | comma-separated capability list, no spaces |

**All caps name receive capabilities. There is no transmit cap, because there is no transmit verb** (§7).

A verb whose capability class is unsatisfied is rejected with `code=nocap` rather than silently ignored, so a mismatched pair is diagnosable from a terminal. The deck greys out UI for absent caps; `OCP_CC_WIFI` is satisfied by **either** `wifi24` or `wifi5`.

### 4.1 Reset announcement

The probe emits an unsolicited `[HELLO]` on boot. The deck treats any `[HELLO]` it did not solicit as: the probe restarted, every cached table is stale, any in-flight command will never be answered, drop back to `Ready(caps)` with state invalidated.

**`[HELLO]` outranks parser state.** A probe can reset *in the middle of* a block frame, so a reader that only accepts the open frame's own tag would swallow the `[HELLO]` and wait forever for an `END` that is never coming. A `[HELLO]` arriving while a frame is open therefore abandons that frame and is delivered. Along with `[EVT]` and `[ERR]`, it is one of three markers accepted in any state.

### 4.2 Connection state machine (deck)

```
Disconnected ──hello──► HelloSent ──[HELLO]──► Ready(caps) ──mode verb──► Busy(mode)
     ▲                      │                      ▲   ▲                      │
     └──── timeout ─────────┘                      │   └──── [STOP] ──────────┘
                          unsolicited [HELLO] ─────┘
```

---

## 5. Replies, events, and errors

### 5.1 Two channels over one wire

The deck's transport layer demultiplexes:

- **Replies** are solicited — routed to the pending-command handler.
- **`[EVT]`** are unsolicited — routed to the current view's subscriber and, when a session is recording, to the logger.

An `[EVT]` may appear **between the `BEGIN` and `END` of a block frame**. A parser must handle interleaving: `[EVT]` and `[ERR]` never belong to an open frame and never terminate one.

Events are lossy by design. When the queue is full the probe drops events rather than stalling the engine, so counts in `[EVT]` are observations, not an audit trail.

### 5.2 Timeouts

Replies are expected within a bounded window; on expiry the deck reports the timeout and returns to `Ready`, then re-syncs on the next `[HELLO]` or successful command. A timeout never leaves a half-read frame in the parser.

**Except for liveness verbs.** `hello` and `ping` exist to prove the probe is there, so when either goes unanswered the deck moves to `Disconnected`, not `Ready`. A slow `scan_networks` is a slow command; a silent `ping` is a missing probe. `reboot` has no reply of its own: the deck waits for the probe's `[HELLO]`, and treats that one as expected rather than as a surprise reset.

### 5.3 Errors

```
[ERR] code=<code> [k=v ...] msg="<human text>"
```

`code` is machine-readable and closed (`OCP_ERR_*` in the header); `msg` is human text and must never be parsed. Codes in v1:

| Code | Meaning | Deck response |
|---|---|---|
| `busy` | PHY lane held; `owner=` names the current owner | offer `stop` |
| `badarg` | wrong arity or an argument out of range | fix the call — a bug |
| `budget` | refused by the power interlock (DESIGN §6.2) | surface; retry after `stop` |
| `hwfault` | peripheral did not respond (e.g. SX1262 BUSY timeout) | surface; the radio is suspect |
| `unknown` | verb not in this build's command table | a version mismatch |
| `nocap` | verb known, capability absent in this build | grey out the UI |
| `internal` | allocation or queue failure | a probe-side bug; report it |

### 5.4 Lifecycle

`stop` is the **universal cancel** and is *always* acked, including when nothing is running:

```
[STOP] running=0 END
```

`running=1` means a mode was cancelled, `running=0` means there was nothing to cancel. Either way the probe is idle when the ack is sent, so the deck can wait for a known state. `status` reports the PHY-lane owner, the LoRa lane state, and uptime.

---

## 6. Text, quoting, and escaping

SSIDs and device names are attacker-controlled byte strings. They can contain quotes, commas, control characters, newlines, and invalid UTF-8. On a line-oriented transport an unescaped newline inside an SSID would desynchronise the parser, so escaping is **mandatory, not cosmetic**.

> *This section resolves a gap in DESIGN §5.1, which requires quoting without specifying an escape.*

**Emitting a text field:**

1. Wrap the field in double quotes.
2. Inside, `\` → `\\` and `"` → `\"`.
3. Any byte outside printable ASCII (`0x20`–`0x7E`) → `\xHH`, lowercase hex. This includes CR, LF, and every non-ASCII byte of a UTF-8 sequence.

The result is always printable ASCII on one line, and is byte-exactly reversible — the deck can reconstruct the original SSID bytes for logging, and a WigleWifi CSV written later re-encodes to that format's own rules.

**Where quoting applies:** all `[SCAN]`-family CSV row fields (always quoted, even numeric ones, so a row is split on `","` boundaries); any `k=v` value containing a space, a quote, or a non-printable byte; and the `msg=` of `[ERR]`.

**Where it does not:** keys, verbs, markers, capability strings, and simple values — these are never quoted. A **key** is `[A-Za-z0-9_.]+`. A **bare value** is `[A-Za-z0-9_.:,+-]*`, which deliberately admits the comma so `caps=wifi24,wifi5` and `set_channels 1,6,11` need no quotes; a CSV row quotes every field, so a comma is never ambiguous there. A value needing any character outside that set is quoted and escaped.

A parser reading an unterminated quoted field discards the line rather than consuming the next one. So does a **malformed escape** in any `k=v` value — a `\` not followed by `"`, `\`, or `xHH` — and the check happens before the line affects parser state, so a malformed `[HELLO]` cannot abandon an open frame. Frame *rows* are delivered raw; a consumer that decodes a row handles its own malformed escapes.

**Decoded bytes stay hostile.** Escaping protects the *transport*; it does not sanitise the content. An SSID is whatever the AP chose to broadcast, so once a field is decoded back to bytes it may contain control characters, ANSI escape sequences, or bidirectional-override codepoints. Every renderer — the deck's views, `ocp_repl.py`, anything reading a log back — must re-escape non-printables before display. Store the true bytes; never print them raw.

---

## 7. Receive-only boundary

**No transmit verb is compiled into any Oscilla build**, so there is no reachable firmware code path to transmission on any radio (DESIGN §8, [D-8](../docs/DECISIONS.md)). Three consequences bind this protocol:

1. **The command table is the whole attack surface.** A capability exists iff its verb is registered. Adding transmit means registering new verbs — a visible, reviewable change, not a runtime toggle.
2. **The deck cannot reach a radio directly.** It speaks only OCP. A compromised or buggy deck cannot inject frames: there is no verb to carry the request and no handler to service it.
3. **No cap advertises transmit**, so no deck build can even present transmit UI.

`ocp.h` refuses to compile if a transmit build flag is defined, and `protocol/test_ocp_header.c` fails if any registered verb matches a transmit-shaped name. The SX1262 is TX-capable silicon; the guarantee is about *reachable code paths*, and is stated that way deliberately.

---

## 8. Worked session

```
                          ets Jul 29 2019 12:21:46      ← ROM boot text, ignored
                          rst:0x1 (POWERON),boot:0xc    ← ignored
[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,wifi5,ble,ieee802154,lora_rx END
> hello
[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,wifi5,ble,ieee802154,lora_rx END
> scan_networks
[SCAN] BEGIN n=2
[SCAN] "1","HomeNet","AA:BB:CC:DD:EE:01","6","WPA2","-52","2.4"
[SCAN] "2","Caf\xc3\xa9","AA:BB:CC:DD:EE:02","36","WPA3","-71","5"
[SCAN] END
> inspect_network 1
[INSPECT] BEGIN idx=1 bssid=AA:BB:CC:DD:EE:01
[INSPECT] mfp_capable=1 mfp_required=0 uptime=1042311
[INSPECT] END
> start_sniffer
[SNIFF] BEGIN ch=1
[SNIFF] END
[EVT] kind=sniff pkts=118 ch=1
[EVT] kind=sniff pkts=349 ch=6
> lora_listen
[ERR] code=budget msg="lora refused while wifi promiscuous is active"
> stop
[STOP] running=1 END
> lora_status
[LORA] BEGIN state=idle
[LORA] freq=0 sf=0 bw=0 cr=0 rx=0 crc_err=0 fault=none
[LORA] END
```

---

## 9. Conformance checklist

A deck-side parser is conformant iff:

- [ ] arbitrary non-marker text between frames is ignored and never advances frame state;
- [ ] an oversized line is discarded without desynchronising the next one;
- [ ] `[EVT]`/`[ERR]` interleaved inside an open block frame are routed out-of-band and do not close it;
- [ ] an unknown marker, unknown `kind=`, or unknown `k=v` key is ignored, never fatal;
- [ ] both compact and block forms are accepted for every tag;
- [ ] a bare `[TAG] END` with no open frame is noise, never an empty frame;
- [ ] a block frame exceeding `OCP_MAX_FRAME_ROWS` is abandoned whole, and the reader recovers;
- [ ] `\xHH`, `\"` and `\\` round-trip to the original bytes;
- [ ] a malformed escape in a `k=v` value makes the line noise, never an exception, and changes no parser state;
- [ ] an unsolicited `[HELLO]` invalidates cached state and cancels any pending command;
- [ ] a `[HELLO]` arriving *inside* an open block frame abandons that frame and is still delivered;
- [ ] a frame missing its `END` is abandoned on timeout without wedging the reader;
- [ ] `pong` is recognised as the reply to `ping`.

`tools/ocp_repl.py --selftest` asserts every item on this list; `tools/ocp_fuzz.py` (P1) replays boot chatter and garbage against it.
