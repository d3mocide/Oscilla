# Oscilla security code audit — 2026-09-16

**Audited revision:** `c665f6048ac69a5411e085fb096d7aebd933b1ad`
**Scope:** `protocol/`, `firmware-c5/`, `firmware-cardputer/`, `tools/`, build
configuration, storage/export paths, and the security/design contract.
**Disposition:** one high-priority memory-safety defect, one high-priority
assurance gap, three medium-priority findings, and five low-priority hardening
or documentation findings. The findings below are the audit snapshot; the
working-tree remediation status is recorded immediately below.

## Remediation status — working-tree follow-up

The follow-up changes remediate A-01 through A-10 in the working tree. Host
regression gates pass for bounded LoRa encoding, receive-only configuration and
opcode allowlists, the fixed nonblocking event queue, transactional OCP
replies, hostile CSV/KML data, strict numeric/GNSS parsing, and fail-closed
Zigbee readiness. Toolchain versions are pinned and build assertions plus a CI
workflow were added. `SECURITY.md` and `WORKLOG.md` contain the implementation
handoff and evidence summary.

The 2026-09-17 bench follow-up partially closed the hardware boundary: the
current images passed 802.15.4 capture/no-ACK observation, Wio lane isolation,
promiscuous sniffing, and three paced UART flood/stop runs. The initial
live-SD-removal gate failed, but the working-tree fix now closes the logger and
marks storage absent on verified failure. The final hardware run showed
`SD: READY` changing to `SD: ABSENT`, then a subsequent explicit logging
request remounted the card and opened the next session without rebooting. A
power cut of only the deck also booted normally, restored the Grove link, and
allocated the next wardrive session after the interrupted one. Full
recovered-file integrity was not inspected byte-for-byte. An independent
802.15.4 positive-control repeat, fully saturated USB-to-Grove flood behavior,
complete Wi-Fi/LoRa receive-only coverage with suitable live sources, host-side
file inspection, and a remote CI result remain open. KML recovery has a
host-tested bounded-prefix model, while the physical SD/remount and
power-cycle paths now have current-image operational evidence.

## Executive summary

The current source contains no transmit verb or transmit-capable call found by
manual review, and the existing receive-only, parser, sanitizer, and firmware
build gates pass. The central receive-only design is sound as an architectural
choice.

The strongest concrete defect is in the LoRa event encoder: a valid
zero-length received packet leaves its `hex` stack buffer uninitialized and
then formats it with `%s`. That can disclose adjacent task-stack contents over
OCP, read out of bounds, or crash the probe. It is reachable from radio input
if the SX1262 reports a zero-length packet.

Two broader guarantees are weaker in implementation than the security contract
states:

1. `check_rx_only.py` is a token denylist, not a proof that radio setup remains
   passive. Removing Wi-Fi's explicit passive initializer silently selects the
   SDK's zero-valued active mode and still passes the check; BLE's `passive`
   field and the SX1262's numeric transmit opcode are likewise not enforced.
2. The promised bounded OCP event queue does not exist as a shared transport
   boundary. Several Wi-Fi and NimBLE callbacks synchronously format and write
   events. RF traffic can therefore apply UART backpressure inside radio stack
   callbacks instead of being dropped at a fixed queue boundary.

The most important next move is to fix A-01, then make the receive-only and
event-backpressure properties executable invariants rather than comments and
identifier conventions.

## Severity model

- **High:** memory safety, unintended radio emission, or a central security
  guarantee that can fail without a red gate.
- **Medium:** remotely triggerable denial of service/data-integrity risk, or a
  hostile export that can become active content in a common consumer.
- **Low:** local/physical-boundary robustness, privacy hardening, build
  reproducibility, or security-document accuracy.

## Findings

### A-01 — High — zero-length LoRa packet uses an uninitialized string

**Evidence:** [`lora_recon.c:122`](../firmware-c5/main/lora_recon.c#L122)
allocates `hex` without initialization. The loop at line 125 writes a terminator
only after encoding at least one byte. A packet with `p->len == 0` skips the
loop, after which line 129 passes the indeterminate buffer to `%s`.

**Impact:** `vsnprintf` reads task-stack bytes until it happens to find NUL. The
result may leak stack contents over the deck link, over-read the stack, emit an
overlong/malformed event, or fault the probe. The protocol and deck model both
allow length zero, so the downstream layers do not make this state impossible.

**Remediation:** initialize `hex[0] = '\0'` before the loop. Prefer extracting a
pure, bounded bytes-to-hex helper that returns success and required length so it
can be host-tested without the ESP-IDF task wrapper.

**Acceptance gate:** an ASan/UBSan host test must encode lengths 0, 1, and 255,
verify exact NUL termination and output length, and go red against the current
implementation. Then run the test on the real firmware path with a synthetic
zero-length `LORA_EVT_PACKET`; no stack bytes may appear after `hex=`.

### A-02 — High — the receive-only check is bypassable by ordinary refactors

**Evidence:** [`check_rx_only.py:20`](../tools/check_rx_only.py#L20) searches for
22 exact identifiers. It does not assert passive configuration values or an
allowlist of SX1262 opcodes.

Concrete gaps:

- `wifi_scan_config_t` is zero-initialized. In ESP-IDF 5.5.1,
  `WIFI_SCAN_TYPE_ACTIVE == 0`. Removing the explicit
  `scan_type = WIFI_SCAN_TYPE_PASSIVE` line therefore enables active scanning
  without introducing the banned `WIFI_SCAN_TYPE_ACTIVE` token.
- [`ble_recon.c:177`](../firmware-c5/main/ble_recon.c#L177) relies on
  `params.passive = 1`, but the check does not require that assignment or reject
  zero.
- The in-house SX1262 driver uses numeric `OP_*` opcodes. Adding
  `OP_SET_TX 0x83` and passing it to `cmd_write()` does not match any of the
  three LoRa strings in the denylist.

**Current exposure:** manual review found the present Wi-Fi and BLE setup
explicitly passive, the 802.15.4 adapter enables promiscuous mode before RX,
and the SX1262 opcode table contains no transmit command. This is an assurance
failure, not evidence that the current image transmits.

**Remediation:** enforce permitted states at the wrappers that own each radio:

- construct Wi-Fi passive scans through one helper and fail the check if any
  scan call bypasses it;
- require `params.passive == 1` at the only BLE discovery wrapper;
- allowlist the exact SX1262 RX/config/read opcodes accepted by `cmd_write` and
  `cmd_read`, explicitly rejecting `SetTx`, `SetTxParams`, and PA configuration;
- keep the existing 802.15.4 promiscuous/auto-ACK guard and hardware no-ACK
  control.

**Acceptance gate:** run the receive-only check against copied fixtures with
each of these mutations: delete Wi-Fi's passive initializer, set BLE passive to
zero, add an SX1262 `0x83` write, call each public 802.15.4 transmit API, and
disable promiscuous mode while enabled. Every mutation must fail. The normal
source and both firmware variants must pass.

### A-03 — Medium — RF callbacks can block on OCP output; the promised event queue is absent

**Contract:** [`ocp.h:50`](../protocol/ocp.h#L50),
[`OCP-SPEC.md:161`](../protocol/OCP-SPEC.md#L161), and
[`SECURITY.md:94`](../SECURITY.md#L94) say events use a fixed-depth queue and
drop instead of blocking an engine.

**Evidence:** `OCP_EVENT_QUEUE_DEPTH` is defined as 32 but unused by firmware.
The following radio-stack callbacks call `ocp_emit_event()` directly:

- [`wifi_deauth.c:40`](../firmware-c5/main/wifi_deauth.c#L40)
- [`wifi_sniff.c:56`](../firmware-c5/main/wifi_sniff.c#L56)
- [`ble_recon.c:110`](../firmware-c5/main/ble_recon.c#L110)

The UART transport calls `uart_write_bytes()` with no timeout at
[`ocp_transport.c:53`](../firmware-c5/main/ocp_transport.c#L53). ESP-IDF's own
5.5.1 UART documentation says that call waits until enough TX ring-buffer space
is available. Thus an event burst can block a Wi-Fi driver or NimBLE host
callback behind the 115200-baud link. Continuous Wi-Fi discovery does use a
nonblocking queue, but it is independently sized to 64 at
[`wifi_networks.c:125`](../firmware-c5/main/wifi_networks.c#L125), not the
contract's shared depth.

**Impact:** a nearby source can generate deauth, probe, or tracker event bursts
faster than UART drains them. Expected consequences are lost radio work,
starvation/latency in the radio stack, delayed `stop`, or watchdog reset. The
watchdog limits the final failure, but does not satisfy drop-not-block.

**Remediation:** introduce one OCP event queue of exactly
`OCP_EVENT_QUEUE_DEPTH`, one lower-priority emitter task, fixed-size event
records, nonblocking producer sends, and per-kind drop counters. No radio
callback should format strings or touch the transport.

**Acceptance gate:** with the transport deliberately held full, inject more
than 32 events from every producer. Callback latency must remain bounded, queue
use must never exceed 32, drop counters must match, `stop` must still land, and
the watchdog must not fire. Repeat on hardware under an authorized RF flood.

### A-04 — Medium — OCP frame locking can interleave or silently deliver partial frames

**Evidence:** [`ocp_frame.c:34`](../firmware-c5/main/ocp_frame.c#L34) ignores
failure to acquire the frame mutex after two seconds and emits anyway. The
per-line mutex at line 51 silently drops a line after 100 ms. A block frame can
therefore lose a row or `END` while later lines continue, or a compact reply
such as `[STOP]` can be emitted inside another task's still-open block frame.

This contradicts [`OCP-SPEC.md:20`](../protocol/OCP-SPEC.md#L20), which requires
an oversized/unavailable frame to be abandoned whole because a truncated
result is indistinguishable from a complete one. A 256-row page at 115200 baud
can itself occupy the wire for roughly two seconds, so the frame-lock timeout
is within normal operating scale rather than purely theoretical.

**Impact:** silent partial tables, swallowed stop acknowledgements, client
timeouts, and stale/incorrect operator views under concurrent output pressure.

**Remediation:** make frame acquisition return success/failure; never emit a
`BEGIN` unless the complete frame owns a transport transaction; and propagate
line-write failure so the producer can abandon/resync explicitly. Preserve the
special out-of-band semantics for `[EVT]`, `[ERR]`, and reset `[HELLO]` through
the bounded queue/arbiter rather than unchecked lock fall-through.

**Acceptance gate:** a deterministic host transport that blocks writes must
prove that no accepted block frame is partial, no second block/compact frame
interleaves, and reset/error/event priority still works. Hold the frame lock
past two seconds and prove the old code goes red.

### A-05 — Medium — hostile SSIDs can become spreadsheet formulas in wardrive CSV

**Evidence:** [`wardrive_csv.cpp:21`](../firmware-cardputer/src/storage/wardrive_csv.cpp#L21)
passes all printable ASCII except comma and quote. Consequently an SSID whose
first byte is `=`, `+`, `-`, or `@` is written at the start of a CSV cell. Those
prefixes are interpreted as formulas by common spreadsheet applications.

**Impact:** a nearby AP can plant active spreadsheet content in a file the
operator later opens. The primary format is intended for WiGLE import, which
reduces likelihood, but the `.csv` extension makes spreadsheet handling
predictable.

**Remediation:** select a neutralization that remains WiGLE-compatible—e.g.
escape formula-leading bytes through the format's existing octal convention.
Document the choice and test the target importer. Do not merely quote the
field; spreadsheet programs may still evaluate quoted formula cells.

**Acceptance gate:** SSIDs beginning with all four formula prefixes, including
leading whitespace variants, must import as literal text in LibreOffice/Excel
and still pass the WiGLE compatibility check. The current exporter should fail
the new adversarial test.

### A-06 — Low — KML hostile-byte and remote-resource handling is incomplete

**Evidence:** [`wardrive_kml.cpp:22`](../firmware-cardputer/src/storage/wardrive_kml.cpp#L22)
escapes XML metacharacters and replaces forbidden C0 controls, but copies every
byte at or above `0x20` unchanged while declaring UTF-8. SSIDs are arbitrary
bytes, so invalid UTF-8 sequences can still make the XML document unparseable.
The five icon styles at lines 70–88 also reference a remote Google asset over
plain HTTP, causing a viewer to make an unnecessary network request.

**Impact:** an AP can corrupt a session's KML for strict consumers. Opening a
valid KML may disclose viewer activity/IP metadata to a third party and fetch
an unauthenticated resource.

**Remediation:** encode hostile text into valid Unicode or a reversible ASCII
representation, and use a self-contained style/KMZ asset with no remote URL.
Also add recovery for a power-cut document that lacks closing tags; incremental
flush alone preserves bytes but not a well-formed KML document.

**Acceptance gate:** generate documents for every byte value and malformed
UTF-8 sequence, then parse the complete output with two independent XML
parsers. Assert that no `http://` or `https://` resource occurs. Simulate power
loss after arbitrary appends and prove the next boot can finalize or recover a
readable document.

### A-07 — Low — command and telemetry numeric validation is inconsistent

**Evidence:**

- [`zig_recon.c:104`](../firmware-c5/main/zig_recon.c#L104) parses without an
  end pointer and casts before range validation. For example, channel `267`
  wraps to 11, and `11junk` is accepted as 11.
- [`lora_recon.c:49`](../firmware-c5/main/lora_recon.c#L49) and line 89 accept
  numeric prefixes with trailing garbage.
- [`ble_recon.c:198`](../firmware-c5/main/ble_recon.c#L198) accepts any
  positive signed-long dwell, allowing a single scan to hold the PHY lane for
  roughly 24 days on a 32-bit target unless stopped.
- [`gnss_model.cpp:18`](../firmware-cardputer/src/model/gnss_model.cpp#L18)
  accepts non-finite altitude/HDOP and does not enforce latitude, longitude,
  or UTC component ranges before logging.

The serial and GNSS wires are explicitly outside the remote trust boundary, so
this is hardening rather than a remote vulnerability. It still weakens the
project's rule that decoded input remains hostile.

**Remediation:** use shared whole-token integer/float parsers with overflow,
finite-value, and range checks; validate before narrowing casts; define a
documented maximum BLE dwell; and validate calendar/time/coordinate ranges.

**Acceptance gate:** table-driven tests must reject empty input, signs where
not allowed, suffixes, overflow, modulo-wrap values, `nan`, `inf`, impossible
coordinates, invalid UTC, and impossible dates.

### A-08 — Low — capability readiness can be advertised after failed 802.15.4 initialization

**Evidence:** [`zig_recon.c:95`](../firmware-c5/main/zig_recon.c#L95) can fail
after creating `s_lock`, but `zig_recon_ready()` at line 102 reports readiness
solely from `s_lock != NULL`. `main.c` logs the initialization failure and
continues, after which `[HELLO]` can still advertise `ieee802154`.

**Impact:** capability negotiation can promise an unusable engine, producing
misleading security/operational state and potentially calling uninitialized
radio resources.

**Remediation:** set a dedicated ready flag only after queue and task creation
both succeed; clean up partial initialization or fail closed permanently.

**Acceptance gate:** inject failure at mutex, queue, radio-init, and task-create
steps. The capability must be absent and every associated verb must return
`nocap` without touching engine state.

### A-09 — Low — build inputs are not fully pinned or continuously enforced

**Evidence:** [`platformio.ini:7`](../firmware-cardputer/platformio.ini#L7)
specifies `platform = espressif32` without a version. The installed resolution
during this audit was 7.0.1, but a clean future build can resolve another
platform/framework/toolchain. ESP-IDF 5.5.1 is documented but not asserted by
the build script, and the generated dependency lock is ignored. No repository
CI workflow runs the security gates.

**Impact:** dependency drift can change radio behavior, compiler protections,
or introduce upstream vulnerabilities while source review remains unchanged.

**Remediation:** pin the PlatformIO platform, assert the accepted ESP-IDF
version during builds, retain a reviewable dependency lock or manifest, and run
`check_protocol.sh` plus both firmware variants in CI. Add a scheduled
dependency-advisory review rather than silently auto-upgrading.

**Acceptance gate:** two clean builds from the same revision resolve identical
versions/hashes, and CI fails on an intentionally changed toolchain version or
receive-only mutation.

### A-10 — Low — `SECURITY.md` materially understates the implemented attack surface

**Evidence:** [`SECURITY.md:123`](../SECURITY.md#L123) says only P0 is complete
and that firmware engines are not written. The repository now contains and has
hardware evidence for Wi-Fi, BLE, 802.15.4, LoRa, GNSS, and storage engines.
The same section says every stated property is enforced today, while A-02
through A-04 show exceptions.

**Impact:** agents and vulnerability reporters can scope review incorrectly,
and the public posture overstates the strength of backpressure enforcement.

**Remediation:** update the implementation-status and enforcement tables after
A-01 through A-04 are triaged. Name the direct-callback and export boundaries
explicitly until fixed.

**Acceptance gate:** reconcile each present-tense claim in `SECURITY.md` with a
code pointer and an automated or hardware gate; label anything else planned or
accepted risk.

## Positive controls and strengths

- The current `OCP_VERB_TABLE` contains 31 verbs and no transmit-shaped verb.
- Manual review found Wi-Fi scans explicitly passive, NimBLE discovery passive,
  802.15.4 promiscuous receive enabled before RX, and no SX1262 TX opcode.
- OCP text encoding is byte-exact and the deck parser is bounded,
  chunk-invariant, reset-aware, and cross-checked against the Python reference.
- Over-the-air 802.11, BLE, and 802.15.4 parsers use fixed bounds and have
  ASan/UBSan fuzz coverage.
- Deck views re-sanitize decoded strings with `ui::printable`; CSV and KML
  already defend against delimiters, XML metacharacters, and C0 controls even
  though A-05/A-06 identify remaining consumer-specific cases.
- Tracked-file inspection found no field capture, private key, credential,
  environment file, firmware binary, or coredump. `.gitignore` correctly
  matched representative CSV, KML, GPX, NMEA, PCAP, and log names.
- Task watchdog panic, compiler stack checking, FreeRTOS stack canaries, and
  disabled coredumps are present in the generated probe configuration.

## Validation performed

| Gate | Result |
|---|---|
| `ASAN_OPTIONS=detect_leaks=0 ./tools/check_protocol.sh` | Pass: all 27 OCP checks; parser fuzz/diff; hostile-frame sanitizer suites; all model/export tests |
| `./tools/build_firmware.sh` | Pass: C5 UART image; Cardputer app, Grove bridge, ADV check |
| `./tools/build_firmware.sh --bench` | Pass: C5 USB image; all three deck environments |
| `git diff --check` | Pass before report creation |
| tracked sensitive-artifact scan + `git check-ignore` samples | No tracked sensitive artifacts found; representative field formats ignored |

The first unmodified protocol run stopped at LeakSanitizer because LSAN cannot
operate under this environment's tracing. Re-running with only leak detection
disabled retained AddressSanitizer and UndefinedBehaviorSanitizer and passed.

## Validation not performed

- No firmware was flashed and no hardware, live RF, UART saturation, stop-under-
  flood, SD removal, or power-cut test was run in this audit.
- No fresh CVE/advisory scan of ESP-IDF, Arduino-ESP32, PlatformIO, M5 libraries,
  compiler tools, or Python/serial dependencies was performed.
- Secure boot, flash encryption, NVS encryption, and signed update/rollback are
  disabled or absent. This matches the documented exclusion of physical-access
  attacks and the lack of secrets on the probe; it is an accepted boundary, not
  counted above as a vulnerability. Revisit if physical tamper resistance or
  trusted firmware provenance enters scope.
- The Grove link and deck debug USB console remain unauthenticated by design and
  are treated as same-enclosure/local interfaces. If either becomes externally
  accessible, the threat model and command authorization must be reopened.

## Recommended order of work

1. Fix A-01 and prove the zero-length regression goes red on old code.
2. Replace the receive-only token heuristic with state/opcode invariants (A-02).
3. Implement one bounded event queue, then repair transactional frame emission
   (A-03/A-04) and run an authorized hardware flood/stop gate.
4. Neutralize hostile export content and add power-cut recovery (A-05/A-06).
5. Centralize strict input parsing and correct readiness (A-07/A-08).
6. Pin build inputs, add CI, and reconcile `SECURITY.md` (A-09/A-10).

Do not mark the audit closed from clean builds alone. Closure requires the
named adversarial test for every finding and hardware evidence wherever the
acceptance gate says hardware.
