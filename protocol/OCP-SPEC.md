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

**`stop` is the one exception to one-at-a-time.** A deck may send it while another command is pending. The cancelled command still gets its reply frame — flagged `aborted=1`, with its unconditional `END` — and then `[STOP] lane=all running=1 END`. A deck therefore waits for both.

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

`stop [lane]` is the **universal cancel** and is *always* acked, including when nothing is running:

```
> stop
[STOP] lane=all running=0 END
```

`running=1` means a mode or command was cancelled, `running=0` means there was nothing to cancel. Either way the addressed lanes are idle when the ack is sent, so the deck can wait for a known state. `status` reports the PHY-lane owner, the LoRa lane state, uptime, and bounded memory telemetry: `heap_total` and `heap` (total and current free default heap), `heap_min` (minimum free default heap since boot), `heap_largest` (largest current free block), and `psram_total`, `psram_free`, `psram_largest` (zero when the board has no PSRAM).

**The optional `lane` argument scopes the cancel** to one of DESIGN §6.2's two arbiter lanes:

| `lane` | Stops | Leaves alone |
|---|---|---|
| `all` (default) | PHY lane *and* LoRa lane | — |
| `phy` | whichever of Wi-Fi / BLE / 802.15.4 holds the PHY | a running LoRa RX session |
| `lora` | the SX1262 RX session | the PHY-lane owner |

A bare `stop` means `all`, so a deck written against an older probe behaves exactly as before. Any other lane value is `[ERR] code=badarg`. The probe echoes what it acted on as `lane=` in the ack; a deck that sees no `lane=` key is talking to a pre-D-16 probe and should assume `all`.

Scoping matters because the two lanes are independent hardware and **may run concurrently** (DESIGN §6.2): an unscoped `stop` sent only to free the PHY for a different Wi-Fi-family engine would otherwise kill an unrelated LoRa session as a side effect ([D-16](../docs/DECISIONS.md)).

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

`ocp.h` refuses to compile if a transmit build flag is defined, and `protocol/test_ocp_header.c` fails if any registered verb matches a transmit-shaped name.

The verb table is not the whole story: an innocent verb could still call a transmitting driver API. `tools/check_rx_only.py` therefore fails the build if probe source references any **transmit-capable API** on a denylist — active Wi-Fi scan, raw 802.11 transmit, association, soft-AP, ESP-NOW, BLE advertising or connection, 802.15.4 transmit. Scans are passive (§10.1). The SX1262 is TX-capable silicon; the guarantee is about *reachable code paths*, and is stated that way deliberately.

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
[SNIFF] ch=1 END
[EVT] kind=sniff pkts=118 ch=1
[EVT] kind=sniff pkts=349 ch=6
> lora_listen
[ERR] code=budget msg="lora refused while wifi promiscuous is active"
> stop
[STOP] lane=all running=1 END
> lora_status
[LORA] running=0 configured=0 rx=0 crc_err=0 header_err=0 irq_drop=0 radio_drop=0 ocp_drop=0 hw_fault=0 END
```

When configured, `[LORA]` also carries `freq`, `sf`, `bw`, and `cr`.
`rx`, `crc_err`, `header_err`, `irq_drop`, `radio_drop`, `ocp_drop`, and
`hw_fault` are monotonic counters for the current listener session. They
identify the radio, queue, OCP, and SPI-transaction boundaries respectively;
an absent packet is not itself an error indication or an RF traffic
measurement. `hw_fault` counts a `GetIrqStatus`/`ClearIrqStatus` SPI
transaction failing while already listening — distinct from `irq_drop`
(the queue between the ISR and the radio task was full) and from the
one-shot `[ERR] hwfault` `lora_listen` can return before a session starts.

`lora_config <freq_hz> <sf> <bw_khz> <cr> [sync_word]` takes an optional
5th argument, decimal 0..255, the LoRa sync word — a receive filter, not
cosmetic: the SX1262 only raises an RX interrupt for a matching sync word.
Omitted, it defaults to `18` (`0x12`), which is also MeshCore's own sync
word, so every caller written before this argument existed keeps behaving
exactly as it did. The `[CFG]` reply and `[LORA]`'s `lora_listen`/
`lora_status` lines all echo the active value back as `sync`.

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

---

## 10. Wi-Fi survey frames

### 10.1 Scans are passive

A scan **listens for beacons on each channel for `dwell_ms`; it never transmits a probe request** (§7). This is slower than an active scan and cannot discover a hidden network's name by probing for it — the price of the receive-only guarantee, not a tuning choice. A dual-band scan therefore replies after several seconds; decks size their timeout for `scan_networks` accordingly.

### 10.2 `scan_networks` and paging

`scan_networks` replies when the scan completes, with the **first page** of results:

```
[SCAN] BEGIN n=3 total=3 first=1 dwell_ms=250 elapsed_ms=9412
[SCAN] "1","HomeNet","aa:bb:cc:dd:ee:01","6","WPA2","-52","2.4"
[SCAN] "2","","aa:bb:cc:dd:ee:02","36","WPA3","-71","5"
[SCAN] "3","Caf\xc3\xa9","aa:bb:cc:dd:ee:03","11","OPEN","-80","2.4"
[SCAN] END
```

| Key | Meaning |
|---|---|
| `n` | rows in *this* frame, ≤ `OCP_MAX_FRAME_ROWS` |
| `total` | results stored on the probe |
| `first` | 1-based `idx` of this frame's first row |
| `aborted` | present as `1` when `stop` cut the scan short |

Rows are ordered by descending RSSI and carry their `idx`; indices stay stable until the next scan. When `first + n - 1 < total`, the deck pages with `show_scan_results <first>`, which returns the next frame from that index. `show_scan_results` with no argument returns the first page. Row columns are always all quoted (§6): `idx`, `ssid`, `bssid` (lowercase, colon-separated), `ch`, `auth` (an `OCP_AUTH_*` value), `rssi` (dBm), `band` (`2.4` or `5`).

### 10.3 `inspect_network <idx>`

Passively captures beacons from one scanned AP on its channel:

```
[INSPECT] BEGIN idx=1 bssid=aa:bb:cc:dd:ee:01 ch=6 band=2.4
[INSPECT] beacons=3 rssi=-51 rsn=1 mfp_capable=1 mfp_required=0 uptime_s=1042311 interval_ms=102
[INSPECT] END
```

- `beacons=0` means none were heard in the capture window; the other row keys are then omitted.
- `rsn=0` means no RSN element (open or WEP); `mfp_*` are then `0`.
- `uptime_s` is the beacon TSF in seconds. Most APs reset it at boot, so it is uptime; some randomise it, so treat it as a hint.
- An `idx` outside the stored results is `code=badarg`.

### 10.4 `start_sniffer`, `show_clients`, `show_probes`

Passively hops a fixed channel list — 2.4 GHz 1–13 plus the non-DFS 5 GHz
channels (36/40/44/48, 149/153/157/161/165), `SNIFF_DWELL_MS` each, plain
round-robin (D-UCB dwell weighting is deferred to P8, D-6; DFS channels are
excluded pending [D-14](../docs/DECISIONS.md) — see `wifi_sniff.c`'s header
comment) — and builds
two in-RAM tables from what it overhears, **transmitting nothing**: which
client MAC talks to which AP BSSID, and which MAC has sent a probe request
for which SSID — the latter is a device revealing its saved-network list
even when it isn't associated to anything, which is the point of running
this passively rather than only scanning associated traffic.

`start_sniffer` replies immediately and the mode continues until `stop`:

```
> start_sniffer
[SNIFF] ch=1 END
[EVT] kind=sniff pkts=118 ch=1
[EVT] kind=client bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a ch=1 rssi=-54
[EVT] kind=probe mac=f4:12:34:56:78:9a ssid="HomeNet" rssi=-61
[EVT] kind=sniff pkts=349 ch=2
> stop
[STOP] lane=all running=1 END
```

| `[EVT] kind=` | Meaning | Keys |
|---|---|---|
| `sniff` | emitted once per channel hop | `pkts` (session total, any accepted frame), `ch` (channel just finished) |
| `client` | a new (not previously seen) AP↔client pairing | `bssid`, `mac`, `ch`, `rssi` |
| `probe` | a new (not previously seen) mac+SSID probe-request pairing; `ssid=""` is a wildcard/broadcast probe | `mac`, `ssid`, `rssi` |

These events fire only on first sighting of a pairing — they are a live feed
of *new* discoveries, not a packet-by-packet trace, and like all events they
are lossy by design (§5.1): a dropped one only delays the deck learning about
it, because `show_clients` / `show_probes` return the full table on demand:

```
[CLIENTS] BEGIN n=1 total=1 elapsed_ms=15234
[CLIENTS] "aa:bb:cc:dd:ee:01","f4:12:34:56:78:9a","1","2.4","-54","128"
[CLIENTS] END

[PROBES] BEGIN n=1 total=1 elapsed_ms=15234
[PROBES] "f4:12:34:56:78:9a","HomeNet","-61","3"
[PROBES] END
```

Columns: `[CLIENTS]` is `bssid`, `mac`, `ch`, `band`, `rssi` (most recent),
`pkts`. `[PROBES]` is `mac`, `ssid`, `rssi` (most recent), `pkts`. Both tables
persist across `stop` (so a session can still be read back afterwards) and
reset on the next `start_sniffer`. Both are capped (`SNIFF_CLIENTS_MAX` /
`SNIFF_PROBES_MAX`) well under `OCP_MAX_FRAME_ROWS`, so unlike `[SCAN]` there
is no paging: once full, new pairings are dropped rather than replacing old
ones — lossy the same way events are, and for the same reason (bounded RAM,
no blocking).

### 10.5 `deauth_detector`

Passively hops the same channel list as `start_sniffer` (§10.4) watching for
802.11 deauthentication and disassociation management frames — receive-only,
defensive monitoring, not to be confused with *sending* a deauth frame (an
offensive, transmit-requiring action this project never does, §7). Replies
immediately and streams for the rest of the session, same shape as
`start_sniffer`:

```
> deauth_detector
[CFG] ch=1 END
[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=0 rssi=-58 ch=6 n=1
[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=0 rssi=-59 ch=6 n=2
> stop
[STOP] lane=all running=1 END
```

Unlike the sniffer's client/probe events (new sightings only), **every**
deauth/disassoc frame produces an event — occurrences, especially a sudden
burst from one `bssid`, are themselves the signal an operator watches for,
not something to deduplicate away. There is no snapshot-dump verb here (no
`show_deauths`): the stream *is* the record, and like all events it is lossy
under load (§5.1) — a real flood will still read as an obvious flood even if
individual frames are dropped from a full queue.

| Key | Meaning |
|---|---|
| `bssid` | addr3 — the network identity the frame claims |
| `mac` | addr1 — the client being dropped (`ff:ff:ff:ff:ff:ff` for a broadcast deauth) |
| `reason` | the reason code exactly as sent — attacker-controlled, not a trustworthy enum |
| `disassoc` | `1` = disassociation, `0` = deauthentication (same detector, both subtypes) |
| `ch` | channel the frame was heard on |
| `n` | running count for this session, so a dropped event doesn't hide *that* something is happening |

### 10.6 `channel_view`, `packet_monitor <ch>`

Both just count frames — `WIFI_PROMIS_FILTER_MASK_ALL`, no address/IE
parsing, no content ever read — so there's nothing here to fuzz the way
`sniff_track`/`deauth_parse` are. Neither has a snapshot-dump verb: the
`[EVT] kind=chan` stream is the whole story, and it's self-healing under
drops, unlike the sniffer's new-pairing-only events — every channel gets a
fresh reading again next cycle, so a lost event just means a stale reading
briefly, not a gap that never fills in.

`channel_view` hops the same list as `start_sniffer`/`deauth_detector`
(§10.4), reporting one live count per channel each time its dwell ends:

```
> channel_view
[CHAN] ch=1 n=22 END
[EVT] kind=chan ch=1 pkts=42
[EVT] kind=chan ch=2 pkts=3
...
> stop
[STOP] lane=all running=1 END
```

`packet_monitor <ch>` locks onto one channel instead of hopping, reporting
packets/s on a 1 s window:

```
> packet_monitor 6
[CFG] ch=6 END
[EVT] kind=chan ch=6 pkts=118
[EVT] kind=chan ch=6 pkts=94
> stop
[STOP] lane=all running=1 END
```

`ch` must be one of the channels this build actually hops (2.4 GHz 1-13,
non-DFS 5 GHz) — anything else is `code=badarg`, checked against the same
list `start_sniffer` uses rather than handed straight to the radio, so an
unsupported channel never silently mislabels frames the way D-14 describes.

### 10.7 `start_wifi_scan`

This is the continuous counterpart to `scan_networks`: it passively hops the
same fixed channel list as `start_sniffer` and `channel_view`, listens only
for beacon and probe-response management frames, and returns immediately.
There are no probe requests and no association. The probe emits one event for
each newly seen BSSID in the session:

```
> start_wifi_scan
[CFG] ch=1 END
[EVT] kind=network bssid=aa:bb:cc:dd:ee:01 ssid="HomeNet" ch=6 band=2.4 rssi=-52 privacy=1 rsn=1 mfp_capable=1 mfp_required=0 interval_ms=102
[EVT] kind=network bssid=aa:bb:cc:dd:ee:02 ssid="" ch=36 band=5 rssi=-71 privacy=1 rsn=1 mfp_capable=1 mfp_required=1 interval_ms=102
```

`bssid` and `ssid` are the observed network identity; an empty SSID means
the beacon carried a hidden SSID. `ch` is the channel currently selected by
the hopper, not an untrusted channel IE. `privacy` is the 802.11 capability
privacy bit, while `rsn` is true only when a bounds-valid RSN element was
present. `mfp_capable` and `mfp_required` are the RSN capability flags, and
`interval_ms` is the beacon interval converted from TU. These fields describe
the beacon's advertised posture; they do not prove authentication, ownership,
or that a network is reachable.

Events are first-sighting-only and lossy like the sniffer's new-pairing
events. The probe's BSSID table and the deck's AP list are bounded; a full
table drops a new BSSID rather than evicting an older one. The mode continues
until `stop`, which disables promiscuous capture and releases `PHY_OWNER_WIFI`.
Use `channel_view` or `packet_monitor` alongside this mode only after stopping
it: all three require the one shared Wi-Fi/BLE/802.15.4 PHY lane.

## 11. BLE frames

### 11.1 Scans are passive here too

`disc_params.passive = 1` on every `ble_gap_disc()` call (§7): the probe
never sends a scan request, so it never reveals itself to a nearby device the
way an active scan's follow-up request would. Same posture as Wi-Fi's
passive-only scanning (§10.1) — for BLE it costs less, since duplicate
advertisements are cheap and there is no hidden-name tradeoff to pay for it.

### 11.2 `scan_bt [dwell_ms]`

Listens across all three advertising channels (37/38/39; the controller
cycles them, not the app) for `dwell_ms` (default `BLE_SCAN_DWELL_MS`,
`ble_recon.c`), deduplicating by device address, then replies with the
device table:

```
[BLE] BEGIN n=2 total=2 dwell_ms=6000 elapsed_ms=6012
[BLE] "f4:12:34:56:78:9a","Pixel Buds","0075","","-58","14"
[BLE] "aa:bb:cc:dd:ee:ff","","004c","airtag","-71","6"
[BLE] END
```

Columns: `mac`, `name` (AD type `0x09`/`0x08`, `""` if absent), `mfr`
(AD type `0xFF` company ID, 4 lowercase hex digits, `""` if absent),
`tracker` (`""` or an `OCP_EVT_KIND_*` tracker value — `airtag` today, see
§11.4), `rssi` (dBm, most recent), `n` (advertisements seen this scan).
Like `[CLIENTS]`/`[PROBES]` (§10.4), the table is capped (`BLE_DEVICES_MAX`)
with no paging — once full, new addresses are dropped rather than replacing
old ones. Each `scan_bt` call starts a fresh table, same as `scan_networks`
starting a fresh result set.

### 11.3 `start_ble_scan`

Same radio, continuous instead of a bounded snapshot — the general-purpose
counterpart to `start_sniffer` (§10.4) rather than to `scan_networks`:
replies immediately and streams for the rest of the session, one event per
**newly seen** address, same "first sighting only" posture as
`start_sniffer`'s `kind=client`/`kind=probe` events (§10.4) — a device
re-advertising doesn't produce a second event, so this is a live feed of
discoveries, not a packet trace:

```
> start_ble_scan
[CFG] BEGIN
[CFG] END
[EVT] kind=ble mac=f4:12:34:56:78:9a name="Pixel Buds" mfr=0075 tracker="" rssi=-58
[EVT] kind=ble mac=aa:bb:cc:dd:ee:ff name="" mfr=004c tracker="airtag" rssi=-71
> stop
[STOP] lane=phy running=1 END
```

Columns match `[BLE]`'s (§11.2) except there is no `n` — the deck already
gets a running count for free by counting the events themselves, since
each one is a distinct new device by construction. There is no snapshot-
dump verb for this session either (no `show_ble_devices`): the event
stream *is* the table, same reasoning `deauth_detector` (§10.5) gives for
skipping one.

### 11.4 `scan_airtag`

Same radio, always-on classification instead of a bounded snapshot: replies
immediately and streams for the rest of the session, watching every
advertisement for Apple's Find My network signature (manufacturer data,
company ID `004c`, payload type byte `0x12` — the AirTag/FindMy-accessory
broadcast, confirmed against public Find My protocol write-ups, not
guessed) regardless of anything `scan_bt`/`start_ble_scan` has or hasn't
seen:

```
> scan_airtag
[CFG] BEGIN
[CFG] END
[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-71 n=1
[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-69 n=2
> stop
[STOP] lane=phy running=1 END
```

| Key | Meaning |
|---|---|
| `mac` | the advertiser's address |
| `rssi` | this sighting's signal strength (dBm) |
| `n` | running count of tracker sightings this session (not unique devices) |

Every matching advertisement produces an event, same posture as
`deauth_detector` (§10.5) — a tracker re-advertising rapidly nearby is
itself part of the signal, not noise to deduplicate away. (`start_ble_scan`,
`scan_airtag`, and `start_antisurveillance` can't run together — all need
`PHY_OWNER_BLE` — but either can run alongside a Wi-Fi-lane engine, same
two-lane arbitration as
everything else on the PHY lane, DESIGN §6.2.)

### 11.5 Tracker classification

`OCP_K_TRACKER` (`[BLE]` rows) and `OCP_EVT_KIND_AIRTAG` (`scan_airtag`
events) share one classifier (`ble_adv_parse.c`) and one v1 scope: Apple's
Find My network only, matching the verb name `scan_airtag` was chosen for.
Other vendors' tracker beacon formats (Tile, Samsung SmartTag, Chipolo) are
a future addition — `OCP_K_TRACKER`'s value is a string specifically so a
new classification is additive, not a wire-format change.

### 11.6 `start_antisurveillance`

This is a defensive session over the same passive BLE tracker classifier. It
acquires `PHY_OWNER_BLE`, sets the controller's discovery parameters to
passive, and streams one `[EVT] kind=airtag mac=... rssi=... n=...` event for
every matching Find My advertisement until `stop`. It intentionally does not
send the deck's GNSS position to the probe: the deck correlates repeated
sightings with its own fresh fixes and keeps that correlation in bounded RAM.

The correlation is conservative and is not proof that a person or device is
being followed. A tracker becomes an on-device candidate only after the same
advertiser is observed at two movement legs of at least 25 m each, with a fix
no older than 10 seconds. No candidate or tracker identifier is written to the
wardrive files by this mode. The command replies with an empty `[CFG]` block
(`[CFG] BEGIN` followed by `[CFG] END`) when the passive session has started;
`[STOP] lane=phy ...` ends it. A bare `[CFG] END` is intentionally not a valid
empty reply: the parser must treat that shape as a late terminator, so a start
acknowledgement cannot be mistaken for noise.

## 12. Passive 802.15.4 frames

`start_zig_recon [ch] [dwell_ms]` acquires the PHY lane and listens on
channels 11–26 (default 11), hopping after `dwell_ms` (50–60000, default 400).
It enables ESP-IDF hardware promiscuous mode before RX; that mode disables
automatic ACK transmission. The teardown disables the subsystem directly and
never changes promiscuous mode back while enabled. There is no association,
commissioning, key handling, network-layer decode, or transmission.

`zig_recon_status` is compact: `[ZIG] state=rx|idle ch= dwell_ms= pans= nodes= END`.
`zig_recon_list` emits the full capped table; `zig_recon_nodes [pan]` filters
node rows by four-lowercase-hex PAN ID; `zig_recon_clear` clears both tables.
Every table frame has `n= pans= nodes= dropped= ch= dwell_ms=`. Rows are CSV:

```
[ZIG] BEGIN n=2 pans=1 nodes=1 dropped=0 ch=11 dwell_ms=400
[ZIG] "pan","1a2b","802154","mac","0001","1","-60","91"
[ZIG] "node","1a2b","1234","","unknown","-60","91","12345"
[ZIG] END
```

PAN columns are `kind,pan,proto,confidence,channels,nodes,rssi,lqi`; node
columns are `kind,pan,short,ext,role,rssi,lqi,seen`. `channels` is a 16-bit
hex mask for channels 11–26. `seen` is the monotonic probe timestamp in ms.
`n` is the number of emitted CSV rows (`pans + nodes`), including both row
types. A `zig_recon_nodes pan` response contains only that PAN and its nodes.
The P7 parser intentionally labels observed MAC traffic only as `802154`:
Zigbee and Thread cannot be safely inferred from a generic MAC header without
the network-layer decoding this slice deliberately excludes. Unknown/invalid
rows are dropped, never rendered as identity claims.
