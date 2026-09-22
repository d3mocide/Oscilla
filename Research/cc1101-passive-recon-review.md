# CC1101 passive-recon review and implementation handoff

**Date:** 2026-09-21  
**Status:** Independent source/datasheet/upstream review; implementation pending  
**Audience:** Claude and later implementation/review agents  
**Scope:** Receive-only CC1101 work on the XIAO ESP32-C5 probe and its Cardputer ADV presentation

## 1. Outcome

The current CC1101 lane is a useful bring-up implementation, but it is not yet
a trustworthy passive-recon receiver. The existing evidence proves SPI access,
the manual-reset path, `PARTNUM=0` / `VERSION=20`, fault-free entry to RX, and
bidirectional exclusion between the SX1262 and CC1101 on real hardware. It does
**not** prove that any observed FIFO bytes came from a real 433 MHz device, that
the current modem profile matches its stated parameters, or that a FIFO drain is
a radio packet.

The immediate direction should be:

1. Correct and test the modem-register calculations.
2. Make the current FIFO-polling path loss-aware and semantically honest.
3. Build an RSSI/carrier-sense activity survey for the existing no-GDO harness.
4. Qualify it against known compatible sources before adding protocol claims.
5. Treat edge-timing capture and general protocol decoding as a later hardware
   revision that requires a resolved spare-GPIO decision for GDO0.

The receive-only invariant remains absolute. Do not copy any upstream transmit,
replay, jamming, brute-force, key-recovery, or emulation functionality.

## 2. Evidence boundary

### Demonstrated on hardware before this review

- CC1101 responded to the bounded reset/ID path with `PARTNUM=0`, `VERSION=20`.
- `legacy_listen` entered RX without a hardware fault after the SO/CS ordering
  defect was fixed.
- LoRa-active blocks CC1101 start and CC1101-active blocks LoRa start.
- The original ungated profile produced 1,095 FIFO-drain events in 20 seconds at
  about -63 dBm. That rate is evidence of activity in the receive pipeline, not
  evidence of real device packets.

### Not demonstrated

- The carrier-sense-gated profile has not run on hardware.
- The deck CC1101 screen and SD session log have not run end to end on hardware.
- No capture has been correlated with a known compatible transmitter.
- No packet boundary, modulation classification, decoded protocol, sensitivity,
  false-positive rate, queue-loss rate, or frequency accuracy is established.
- GPIO7 cold boot, native-USB recovery, JTAG recovery, shared-MISO isolation,
  RF coexistence, and power gates remain as stated in
  `docs/hardware/c5-dual-radio-wiring.md`.

Software/build evidence is not RF evidence. Keep these categories separate in
future WORKLOG and ROADMAP updates.

## 3. Current implementation findings

### P0 — modem profile does not match its comments

`firmware-c5/main/cc1101_radio.c` writes:

```c
write_reg(REG_MDMCFG4, 0x06);
write_reg(REG_MDMCFG3, 0x00);
```

For a 26 MHz crystal, the CC1101 datasheet formulas give:

- `DRATE_E=6`, `DRATE_M=0` → approximately **1,586.9 baud**, not 2.4 kbaud.
- `CHANBW_E=0`, `CHANBW_M=0` → approximately **812.5 kHz**, not a narrow or
  reset-default channel filter.

The reset bandwidth fields are `CHANBW_E=2`, `CHANBW_M=0`, approximately
203.1 kHz. The current 812.5 kHz receiver window is broader than Flipper's
documented 650 kHz wide OOK preset and is expected to admit substantially more
unwanted energy.

Do not replace `0x06` with another unexplained literal. Introduce named field
encoders or a small typed preset table, derive values from the TI formulas, and
test the derived effective baud rate and bandwidth on the host. TI recommends
SmartRF Studio for optimum register sets and especially for AGC/MAGN_TARGET
selection; any generated values must be recorded with their inputs and reviewed
against the datasheet before landing.

### P0 — FIFO drains are not packets

The radio uses infinite-length FIFO mode, no known preamble/sync word, no CRC,
and a 20 ms polling task. Each emitted `CC1101_EVT_CHUNK` is simply the bytes
present when the poll ran. It has no demonstrated relationship to a transmitter's
packet boundary.

The deck currently labels these records `PACKETS`, stores them as
`LegacyPacket`, and writes packet-like CSV rows. Until real framing exists, use
truthful names such as `slice`, `chunk`, or `burst candidate`. A gap- or
carrier-sense-derived burst remains a candidate, not a decoded packet.

RSSI is read after the FIFO data. In this mode it is a live snapshot at drain
time, not a per-packet RSSI and potentially not the peak of the burst that
generated the bytes.

### P1 — RX FIFO polling can duplicate data

The driver reads `RXBYTES` once and drains every reported byte. TI documents a
race when the last FIFO byte is read at the same time a new RF byte arrives: the
FIFO pointer may not update correctly and the last byte can be duplicated. For
streaming packets longer than the FIFO, TI recommends reading `RXBYTES` until
the same value is returned twice and draining `n - 1` bytes while reception is
still in progress.

Implement a bounded stable-count helper and retain one byte during continuous
RX. Add a host-testable pure function for the drain-count decision. Never spin
indefinitely waiting for a stable count.

### P1 — capture loss and stale events are invisible

The radio queue holds eight events. `xQueueSend(..., 0)` failures are ignored,
so overload silently loses data. The queue is not reset before a new capture,
so stopped-session records can be emitted after a restart or frequency change.

Required behavior:

- Clear the event queue before setting `s_running=true` for a new session.
- Count and expose hardware FIFO overflows, radio-queue drops, OCP event drops,
  and deck-model drops separately.
- Add a monotonically increasing session identifier to prevent stale events
  from being attributed to the new frequency/profile.
- Include frequency and profile identifier in every observation record or bind
  them unambiguously through a session-start record.

### P1 — carrier-sense claims are stronger than the configuration evidence

The new relative 10 dB carrier-sense threshold is a sensible experiment, but it
has not been flashed. `AGCCTRL1=0x60` enables the relative threshold while
leaving the absolute threshold field at zero, not disabled. The TI datasheet
describes absolute and relative carrier-sense conditions as separately
adjustable. Do not describe this profile as purely self-calibrating until the
actual `PKTSTATUS.CS`, RSSI, noise floor, and threshold behavior are measured.

An activity-survey event should carry at least:

- configured frequency and profile;
- settled noise-floor estimate;
- peak and last RSSI;
- carrier-sense assertion count and occupied time;
- capture duration;
- FIFO/queue/OCP drop and overflow counts.

### P1 — LoRa task-allocation failure leaks the sub-GHz owner

In `firmware-c5/main/lora_recon.c`, failure to create the drain task stops the
SX1262 but does not release `SUBGHZ_OWNER_LORA`. This can leave both sub-GHz
start paths blocked until reboot. Add an adversarial lifecycle test and release
the owner on that failure path.

### P2 — documented transition behavior and implementation differ

The wiring authority says a transition to one sub-GHz receiver first tears the
other down. The implementation and ROADMAP currently describe/refuse a cross-lane
start with `busy`. Either behavior can be defensible, but the contract must be
one thing. Do not change it silently: reconcile DESIGN, the wiring authority,
the OCP behavior, and the deck workflow in one focused change.

## 4. Upstream comparison

| Area | Oscilla now | Flipper Zero | Bruce | rtl_433_ESP lesson |
|---|---|---|---|---|
| Unknown-signal capture | Polled FIFO bytes | GDO level/duration stream | GDO0 interrupt/RMT raw timings | Pulse trains feed demodulators/decoders |
| Profiles | One fixed OOK profile | Named OOK/2-FSK/GFSK presets plus custom register sets | Runtime frequency, bandwidth, modulation, and threshold choices | OOK and FSK are distinct receiver modes |
| Discovery | One configured frequency | Analyzer and small hopping lists | RSSI threshold sweep and spectrum views | Long hops miss intermittent devices |
| Decode | None | Decoder registry fed with every edge duration | RCSwitch/raw plus additional decoders | Broad device coverage follows a normalized pulse representation |
| Loss handling | Hardware overflow only; software drops silent | Stream-buffer overrun and short-pulse filtering | RMT/ISR path; source documents noise/UI pressure | Loss/overrun must remain visible |
| Stored artifact | FIFO hex labelled as packets | Frequency + preset + decoded fields or raw timings | Flipper-like `.sub` raw/decoded files | Raw captures enable repeatable offline tests |

### Flipper Zero

The useful design is the separation between radio presets, raw edge capture,
and protocol decoders. Its worker receives `(level, duration)` pairs, filters
very short pulses, buffers them, reports overrun, and feeds eligible decoders.
Documented stock presets include OOK at 270/650 kHz and multiple asynchronous
2-FSK deviations. Custom presets preserve the exact CC1101 register set with
the recording.

Apply the architecture, not the transmit/replay feature set and not copied
source. Sources:

- <https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/subghz/subghz_worker.c>
- <https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/subghz/receiver.c>
- <https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/file_formats/SubGhzFileFormats.md>

### Bruce

Bruce demonstrates two capabilities that fit the present harness:

- frequency-range discovery using RSSI and a user threshold;
- a spectrum/activity view with peak hold and decay.

Its raw-capture path uses GDO0 edge timing rather than pretending arbitrary
FIFO bytes are packets. The source also records practical failure modes around
interrupt storms, UI redraw pressure, shared SPI, and watchdog starvation. For
Oscilla, radio acquisition and rendering must stay decoupled through bounded
queues and counters.

Bruce's scanner uses broad bandwidth and simple thresholds as pragmatic
defaults. Those numbers are not authoritative for Oscilla's module, antenna,
noise floor, or desired sensitivity; benchmark rather than copying them.

- <https://github.com/BruceDevices/firmware/tree/main/src/modules/rf>
- <https://github.com/BruceDevices/firmware/wiki/RF>

### rtl_433 and rtl_433_ESP

These projects demonstrate the long-term passive-recon value: normalized OOK
and FSK pulse trains can support weather sensors, thermometers, alarm contacts,
doorbells, remotes, TPMS, and other telemetry decoders. `rtl_433_ESP` explicitly
notes that OOK and FSK cannot be demodulated simultaneously and that its CC1101
receiver is less sensitive than an SDR in the author's testing.

Do not import the decoder collection wholesale. Start with captured fixtures
for actual local device classes and a small original decoder surface.

- <https://github.com/merbanan/rtl_433>
- <https://github.com/NorthernMan54/rtl_433_ESP>

## 5. Hardware and frequency limits

The selected E07-M1101D-SMA module is specified for **387–464 MHz**. Do not
inherit Flipper Zero's 300–348 MHz or 779–928 MHz claims merely because the
underlying CC1101 family can be designed for those bands.

Realistic targets for this Oscilla module include compatible 433.92 MHz:

- weather, temperature, humidity, rain, and pool sensors;
- doorbells, simple remotes, and wireless switch/contact sensors;
- some regional alarm/PIR devices;
- some 433 MHz TPMS and telemetry devices.

Common North American 315, 319.5, and 345 MHz sensors are outside the module's
range. So are 868/915 MHz FSK devices. The SX1262 covers LoRa observation in its
own supported band; that does not make it a general FSK or raw-IQ receiver.

Frequency alone never identifies a protocol. A peak at 433.92 MHz is an RF
activity observation until modulation and framing are demonstrated.

## 6. Recommended implementation sequence

### Workstream A — make the existing polling lane correct

Write adversarial tests first, then:

1. Add a typed, receive-only `cc1101_rx_profile_t` containing modulation,
   effective data rate, filter bandwidth, deviation where applicable,
   sync/carrier policy, and the reviewed register values.
2. Add pure host-tested calculations or validation for frequency word, data
   rate, channel bandwidth, and RSSI conversion.
3. Replace the current mismatched literal profile with one datasheet/SmartRF-
   traceable starting profile; keep the chosen values explicit in status.
4. Implement stable bounded `RXBYTES` sampling and the continuous-RX `n - 1`
   drain rule.
5. Clear stale events at session start and expose all drop/overflow counters.
6. Rename packet-shaped types/UI/log fields to truthful chunk/slice terminology.
7. Fix the LoRa owner leak and settle the reject-versus-transition contract.

Do not add a decoder in this workstream. Its exit gate is a correct and
observable capture substrate, not a protocol claim.

### Workstream B — activity survey on the current harness

Add a bounded receive-only survey that uses RSSI and `PKTSTATUS.CS`, not FIFO
volume alone:

1. Support a small explicit list/range of legal receive frequencies within
   387–464 MHz and a selected profile.
2. Tune, settle, sample RSSI/CS for a bounded dwell, and move on.
3. Track noise floor, peak, occupancy, hit count, and loss counters per bin.
4. Provide fixed-frequency watch after discovery; long continuous sweeps will
   miss short, infrequent bursts.
5. Present it as **433 Activity Survey** or **Signal Activity**, not a spectrum
   analyzer or protocol scanner. CC1101 is a narrowband tuner, not raw IQ.

The survey must remain responsive to scoped `stop legacy` and must never starve
the C5 dispatch task.

### Workstream C — physical qualification

Use a known compatible, independently operated source. Oscilla itself remains
receive-only. Record:

- source identity/class without committing sensitive field data;
- configured frequency, modulation, deviation/data rate, bandwidth, profile;
- antenna, distance, orientation, and capture duration;
- source-present versus source-absent event/false-positive rates;
- RSSI/noise/occupancy distributions and all drop/overflow counters;
- repeated start/stop, LoRa/CC1101 exclusion, cold boot, USB/JTAG recovery;
- deck rendering and SD log inspection with real sparse and overload cases.

Do not call reception proven merely because a nearby control was pressed and a
counter changed. The observed timing/payload must correlate repeatably with the
known source and disappear or change as expected in the negative control.

### Workstream D — future GDO0 hardware revision

General unknown-protocol capture needs edge timing. The first harness explicitly
leaves GDO0/GDO2 open and the authoritative pin plan has no approved spare C5
GPIO. This is blocked on a new decision; do not borrow Wio DIO1, reset, BUSY,
RF_SW, UART, or a strap-sensitive pin by guesswork.

After a pin allocation is formally resolved:

1. Configure asynchronous receive output on dedicated GDO0.
2. Use an ESP timing/RMT peripheral to capture edge level/duration records.
3. Keep the ISR minimal: timestamp/enqueue only, with no SPI or formatting.
4. Add glitch filtering, bounded buffers, explicit overrun markers, and a gap-
   delimited burst builder.
5. Preserve raw timings with frequency/profile provenance for replay-free,
   offline regression fixtures.
6. Add small, original, fixture-driven decoders only after raw capture is
   physically proven.

## 7. Protocol and UI implications

`protocol/ocp.h` remains the sole owner of literals. If the event shape changes,
update the C5 producer, deck model, tools, protocol spec, and hostile parser
fixtures together.

Recommended conceptual records are:

- session start/status: frequency, profile, effective modem values, session ID;
- activity bin: frequency, dwell, noise, peak, CS occupancy/hits;
- raw slice/burst candidate: session ID, frequency/profile, timing or bytes,
  RSSI metadata, overflow/drop flags;
- decoded observation, later only: protocol, validated fields, confidence or
  validation status, and source raw-record reference.

The deck must distinguish `LIVE`, `CACHED`, and `UNVERIFIED`. Raw bytes must be
escaped at every renderer. SD logs are field data under `SECURITY.md` and never
belong in the repository.

## 8. Licensing and provenance

Oscilla is MIT. Flipper firmware is GPLv3, Bruce is AGPLv3, and rtl_433 is
GPLv2-or-later. Their source is appropriate for architectural research, but
copying their workers or decoders into Oscilla would create licensing and
provenance obligations that this handoff does not authorize.

Implement original code from the TI datasheet, public protocol specifications,
and repository-owned test captures. If third-party code is ever deliberately
adopted, stop for license review, isolate it in `firmware-c5/components/`, retain
headers, and update `NOTICE` in the same change as required by AGENTS.md.

- <https://github.com/flipperdevices/flipperzero-firmware/blob/dev/LICENSE>
- <https://github.com/BruceDevices/firmware/blob/main/LICENSE>
- <https://github.com/merbanan/rtl_433/blob/master/COPYING>

## 9. Acceptance gates

### Host/source gates

- [ ] Register-calculation tests prove the chosen frequency, data rate, bandwidth,
  deviation, modulation, and sync/carrier fields.
- [ ] Mutation tests show the checks fail for the current `0x06`/`0x00`
  mischaracterization and for a transmit-shaped CC1101 API/strobe.
- [ ] FIFO stable-count, `n - 1`, overflow, stale-session, queue-full, and stop
  lifecycle tests pass.
- [ ] OCP/deck tests reject malformed lengths, profile IDs, session IDs, RSSI,
  counters, and raw records.
- [ ] UI/log terminology contains no packet claim for unframed slices.
- [ ] `python3 tools/legacy_bench.py --selftest` passes.
- [ ] `ASAN_OPTIONS=detect_leaks=0 ./tools/check_protocol.sh` passes.
- [ ] `python3 tools/ocp_repl.py --selftest` passes.
- [ ] `./tools/build_firmware.sh` passes for the probe and deck.
- [ ] `git diff --check` is clean.

### Hardware gates on the existing harness

- [ ] Carrier-sense profile is flashed and register readback matches the intended
  profile, including `IOCFG1.GDO1_CFG=0x2E`.
- [ ] Known-source and no-source controls establish a repeatable signal/noise
  difference without continuous false events.
- [ ] Frequency accuracy and useful bandwidth are checked against a known source.
- [ ] FIFO, queue, OCP, and deck drop/overflow counters stay understood during
  quiet, normal, and overload cases.
- [ ] `stop legacy` is prompt during silence and activity.
- [ ] LoRa ↔ CC1101 exclusion/transition behavior matches the chosen contract.
- [ ] Real CC1101 observations render correctly and SD logs parse after remount.
- [ ] Cold boot, native USB, JTAG recovery, shared-MISO isolation, coexistence,
  and power gates in the wiring authority pass.

### Future GDO0 gates

- [ ] A new D-n decision and authoritative wiring revision allocate GDO0.
- [ ] Logic-analyzer evidence matches captured edge timings for a known source.
- [ ] Glitch filter and overrun markers go red under adversarial fixtures.
- [ ] At least one original decoder passes recorded positive, negative, truncated,
  jittered, repeated, and checksum-failure fixtures.
- [ ] No transmit verb, strobe, driver API, replay path, or key-recovery surface
  is introduced.

## 10. Recommended commit boundaries

Keep reviewable changes focused:

1. Register/profile math and tests.
2. FIFO correctness, loss counters, and lifecycle regressions.
3. OCP/model/UI terminology migration.
4. RSSI/carrier-sense activity survey.
5. Hardware evidence and documentation updates.
6. Future GDO0 decision/wiring, then capture substrate, then decoders.

Do not combine a hardware pin-plan decision, protocol expansion, UI redesign,
and decoder import into one commit. Do not move D-15 to resolved until the
applicable physical gates above are recorded.

## 11. Primary sources

- Texas Instruments, *CC1101 Low-Power Sub-1 GHz RF Transceiver*, Rev I:
  <https://www.ti.com/lit/ds/symlink/cc1101.pdf>
- Oscilla wiring authority: `docs/hardware/c5-dual-radio-wiring.md`
- Oscilla baseline hardware authority: `Research/c5-backpack-design.md`
- Flipper Sub-GHz implementation and format sources linked in §4.
- Bruce RF implementation and wiki linked in §4.
- rtl_433 and rtl_433_ESP sources linked in §4.

