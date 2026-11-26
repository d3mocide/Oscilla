# Oscilla — Worklog

> Append-only, newest first. One entry per working session or notable event: what was done, what was decided, what surprised us, what the bench showed. Keep it factual — this is the project's memory. Cross-reference `D-n` ([`docs/DECISIONS.md`](docs/DECISIONS.md)) and phases ([`ROADMAP.md`](ROADMAP.md)).

**Entry template:**
```
## YYYY-MM-DD — <short title>
**Phase:** Pn · **By:** <who>
- What happened / what changed.
- Decisions touched: D-n → …
- Surprises / open threads.
```

Earlier months: [October 2026](docs/worklog/2026-10.md), [September 2026](docs/worklog/2026-09.md). Each month moves there once it ends.

---

## 2026-11-26 — v0.4.0 milestone cut

**Phase:** release · **By:** Claude + Will

- Second tagged version, 66 commits after v0.3.0. Will's uncommitted work was
  committed first in three groups: the deck key guide, audio cues and wake-key
  handling (`e221395`); worklog/roadmap notes (`ef2aa92`); and the hardware
  worklog (`bd8158f`). Then the release commit: `VERSION` 0.4.0, `CHANGELOG.md`,
  ROADMAP header. OCP stays at 1: the new verbs and the `rst` field are additive.
- Gates on the release tree, with Will's work included:
  `ASAN_OPTIONS=detect_leaks=0 check_protocol.sh`, `ocp_repl.py --selftest`,
  `build_firmware.sh` (both firmwares) and `--bench --probe-only`,
  `check_ui_palette`, `check_rx_only` and `check_deck_stack` all pass.
  `render_external_views.py --sync-guide-assets` regenerated the same six deck
  PNGs Will had already updated, and nothing else.
- Committed tree alone (before Will's work was added): a clean checkout of
  `c0e640b` passed `check_protocol.sh`, the selftest and both firmware builds.
- Units before the cut: about 4 h on the pre-release images after the synth
  run, with 0 link errors or timeouts. Deck free-heap low 69.1 KB, set during
  the synth run; probe low 91.2 KB.
- **Not yet flashed.** All three USB devices (probe, deck, Flipper) dropped
  off the dock's hub before the flash started, so neither board took the
  0.4.0 image. The deck still runs the pre-release working-tree build and the
  probe runs the pre-release `build-uart`. Flash both from the tag and check
  that `[HELLO]` reports `ver=0.4.0` (RELEASING step 7).
- The deck SD holds the synth-run drive (`SYNTH-` / `02:5e:` rows mixed with
  real networks). It must be deleted and never uploaded.

## 2026-11-25 — Tables pushed past their caps on hardware with synthetic events

**Phase:** stability · **By:** Claude + Will

- **Synth bench build** (`--synth-bench`, `CONFIG_OSCILLA_SYNTH_BENCH`, default off):
  OCP on Grove as in the field, plus a USB console on the probe
  (`synth <wifi_n> <ble_n> <seconds> [keep]`) that injects made-up `network` and
  `ble` events into the normal event queue. Nothing is transmitted and no OCP verb
  is added. Addresses are locally administered `02:5e:00/01:xx:xx:xx`, SSIDs/names
  `SYNTH-nnnnnn`, so they can't pass for real devices. The first `keep` of each lane
  are re-reported every 5 s. `build_firmware.sh` fails any other build that has the
  console on (checked with a forced-on config). Host test `synth_plan_test`: even
  Wi-Fi/BLE spread, unique addresses; mutants (Wi-Fi first, lane byte dropped) fail.
  Chosen over Flipper beacon/BLE spam, which would reach everyone nearby.
- **Run** (`tools/deck_synth_caps.py`, 600 networks + 300 devices over 10 min,
  keep 8, wardrive session open, real traffic on top). **Completed:** no resets, 0
  link errors/timeouts/resets, 0 events rejected or dropped on the probe.
  - Wi-Fi list hit 256 at ~2 min and held there; `scan_evicted` 548 (≈ 600
    synthetic + ~190 real − 256).
  - BLE list hit 96 at ~45 s and held; `bt_evicted` 882 (real BLE address churn
    adds to the 300).
  - **Wardrive kept logging past both caps** (the D-22 bug): AP rows 22 → 4,254
    and BLE rows 0 → 2,783, still climbing at the same rate after the lists were
    full. Pending queue stayed at 0–3.
  - Deck: free min 69.1 KB, largest block min 54.3 KB with both lists full
    (`tbl_scan` 36 KB at 256 rows, `tbl_bt` 15 KB). Probe free min 81.6 KB.
- The deck had a GNSS fix, so the synthetic rows were written to that drive's SD
  files alongside real nearby networks. That drive must be deleted, never uploaded.
- `dump` adds `wardrive_aps` and `wardrive_aps_nofix`. Output:
  `output/synth/20261125-203418/` (git-ignored).

## 2026-11-25 — Combined 2 h run passes with D-22 + D-23

**Phase:** stability · **By:** Claude + Will

- `deck_stability.py`, default stages (45 min all lanes, 30 min mode switching,
  4 × use/idle), deck `259f8e3`, probe `d2edbfb` on its Grove build. **Completed:**
  0 resets, 0 link errors/timeouts/resets, 0 UART or SD write errors; loop stack
  ≥ 2,592 B; max loop cycle 523 ms (run 2: 442).
- Compared with the D-21 runs:

  | | Run 1 (`129e14f`) | Run 2 (`c4a811a`) | This run |
  |---|---|---|---|
  | Deck largest block, min | 12.5 KB | 25.1 KB | **36.9 KB** |
  | Deck free, min | 40.9 KB | 56.6 KB | **70.2 KB** |
  | Probe free, min | ~9 KB (sniffer) | ~9 KB (sniffer) | **85.0 KB** |

  The deck's largest block steps down as each mode leaves its table cached (scan
  19–22 KB, BLE 22–23 KB, contacts up to 11.7 KB), then holds at 36.9 KB from the
  second use/idle cycle to the end while free heap recovers to 89 KB: a plateau,
  not a leak. Baseline held at 2.3 KB (130 APs) throughout.
- **Not exercised on hardware:** the bench air peaked at 158 networks and 89 BLE
  devices, under the 256 / 96 caps, so `scan_evicted` and `bt_evicted` stayed 0. The
  replace-when-full path rests on the host tests (600-network and 300-device drives).
  A real drive, or a busier site, is what would reach it.
- `deck_stability.py` now records `scan_evicted` and `bt_evicted`. Output:
  `output/deck-stability/20261125-150049/` (git-ignored).

## 2026-11-25 — Probe Wi-Fi driver buffers move to PSRAM (D-23)

**Phase:** stability · **By:** Claude + Will

- **Fix for the 8.5 KB low-water below.** `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` in
  `sdkconfig.defaults`. The same 45 min sniffer + LoRa + OOK bench run now bottoms at
  **86,188 B** (was 8,508 B), within 140 B of where it started, with no resets. Idle
  internal free: 87,188 → 93,792 B. On the Grove build through the deck:
  95.8 KB free, 91.2 KB min, link clean.
- **Side effects, pinned.** The option forces static TX buffers, 16 by default, which
  cost ~26 KB internal and left the first test image *worse* than before (69 KB idle);
  the probe never sends data frames, so 2. A fresh config also moved static RX buffers
  10→16, the BA window 6→16 and TCP out-of-order pbufs 4→0; all are pinned back so
  the default build is byte-for-byte the config that was measured.
- **A/B, 6 alternating 10 min slots on the bench** (5 min Wi-Fi-only wardrive, then
  5 min sniffer; counts only):

  | | Current (A) | PSRAM (B) |
  |---|---|---|
  | Wardrive unique networks | 160, 165, 165 (163.3) | 157, 162, 160 (159.7) |
  | Wardrive network events, mean | 1,399 | 1,296 |
  | Sniffer frames/s, mean | 84.6 | 83.8 |
  | Sniffer clients / probers, mean | 77 / 42 | 75 / 46 |
  | Lowest internal free | 48,032 B | 89,212 B |

  Sniffer capture is unchanged. Wardrive came in ~2% lower on unique networks and
  ~7% on events, below both neighbouring A slots every time: small but consistent.
  Likely cause, unconfirmed: wardrive hops every 200 ms, so any extra latency right
  after a hop costs a beacon or two per channel. Will accepted the trade for the
  stability and headroom. Next candidate: PSRAM at 80 MHz (now 40, ESP-IDF's C5
  default), which needs its own A/B.
- **PSRAM 80 MHz: no edge, stays at 40.** Test image only (one setting changed).
  Boot timing tuning and the 8 MB memory test pass at 80 MHz. 4 alternating pairs of
  5 min Wi-Fi-only wardrive, 40 MHz (A) vs 80 MHz (B):

  | | 40 MHz (A) | 80 MHz (B) |
  |---|---|---|
  | Unique networks | 165, 160, 155, 159 (159.8) | 169, 153, 159, 161 (160.5) |
  | Network events, mean | 1,252 | 1,273 |
  | Lowest internal free | 89,212 B | 87,580 B |

  The difference (+0.7 networks, +1.7% events) is well inside the ±5 swing between
  neighbouring slots, so doubling PSRAM speed does not recover the ~2% wardrive gap;
  bandwidth is not the cause. No resets or errors. Output: `output/psram/ab80.jsonl`.
- Output: `output/psram/run45.log`, `output/psram/ab.jsonl` (git-ignored).
  `check_protocol.sh`, `ocp_repl.py --selftest` and `build_firmware.sh` pass.
  Not yet in a combined deck + probe run.

## 2026-11-25 — Probe low-water traced to Wi-Fi RX buffers; deck live tables cap at 256 and replace; baseline shrinks 5×

**Phase:** stability · **By:** Claude + Will

- **Probe 9 KB low-water: cause found, fix not chosen.** A temporary heap-hook bench
  build (`CONFIG_HEAP_USE_HOOKS`, not committed) recorded which task made each new
  internal-heap low. In a 45 min sniffer + LoRa + OOK run the low-water fell
  72 → 50 → **8.5 KB** at 31 min (sampled free heap never moved: 75.0 KB). Every
  allocation at the lows came from the Wi-Fi driver task in triples of
  1,856 + 168 + 8 B per received frame, with bursts of up to 3.4 MB/s of
  allocate/free. 32 queued frames (`DYNAMIC_RX_BUFFER_NUM`) × 2,032 B ≈ 65 KB, and
  75 − 65 ≈ 10 KB, matching the low. So the floor is bounded by the RX buffer count,
  not a leak, but at that moment anything else on the probe has ~8 KB. The combined
  deck runs never showed it because their sniffer slots were 2.5 min. Options
  (radio-affecting, Will's call): move Wi-Fi buffers to PSRAM
  (`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`), or lower `DYNAMIC_RX_BUFFER_NUM`
  (drops more frames in bursts). Output: `output/heapdiag/run45.log` (git-ignored).
- **Deck live tables (D-22).** Wi-Fi cap 512 → 256, matching the probe's scan store;
  BLE stays 96. A full live/wardrive table now replaces its longest-unseen row in
  place instead of refusing newcomers. Found on the way: wardrive logs only rows
  present in these tables, which reset only at drive start, so the SD log stopped
  taking new BLE devices after 96 and new APs after 512. `dump` adds `scan_evicted`
  and `bt_evicted`.
- **Baseline APs are 12 bytes** (address bytes, auth code, SSID offset into one pool);
  file format unchanged. The deck's saved 130-AP baseline: 12.4 KB → **2.3 KB**. Deck
  heap 17 s after boot: 165 KB free, 147 KB largest.
- Tests: 600-network and 300-device drives log every newcomer, keep a still-heard row
  and hold the cap; byte-exact baseline round trip (escaped, hidden, unlisted auth);
  case-insensitive address match. Mutants (refuse when full ×2, no pool trim, pool kept
  on clear, case-sensitive match) all fail. `check_protocol.sh` and both firmware
  builds pass. Both units flashed (probe back on `build-uart`). Not yet in a
  combined run.

## 2026-11-25 — Deck memory: the external panel stops copying models; two full 2 h combined runs pass

**Phase:** stability · **By:** Claude + Will

- **External panel borrows, doesn't copy (D-21, `129e14f`).** `tickExternalGpsDisplay`
  holds pointers to the `DeckApp` models for a frame instead of per-frame copies. The
  copies doubled the largest tables and were freed and rebuilt on every view change,
  which is what fragmented the heap into the 2026-11-24 abort. Cost accepted: a frame
  drawn across ticks can tear once (~50 ms) if a table changes mid-frame.
  `check_external_frame_memory.py` tours 300 APs / 96 BLE / 200 clients and requires
  every display allocation < 1 KiB with none retained (measured 31 B, 0). Mutants
  restoring a BLE copy (15 KB block) or a scan copy (698 retained) fail.
- **Run 1** (`deck_stability.py`, 2 h, `129e14f`): completed. 0 resets, 0 link errors;
  BLE live, the old abort point, passed. But the largest block fell to **12.5 KB**
  (free min 41 KB): fragmentation was reduced, not gone.
- **Two tables sized once (`c4a811a`).** BLE live discovery grew `devices_` by doubling
  (old and new blocks held together, ~15 KB at the last step); it now reserves its
  96-device cap in one step. The Wi-Fi baseline is loaded from SD at boot and held all
  session: 130 APs sat in a 256-slot block (22.0 KB); it now reserves what it will hold,
  trims, and releases on clear (12.4 KB). Host tests fail with either fix removed.
  `dump` gained `baseline_aps`, `anti_trackers`, `bt_hits` and `tbl_*` per-table heap
  estimates; `deck_stability.py` records them. Deck boot heap 138/115 KB → 153/139 KB
  (free/largest) across both commits.
- **Run 2** (2 h, `c4a811a`): completed. 0 resets, 0 link errors, stack ≥ 2,592 B,
  max loop cycle 442 ms. Largest block min **25.1 KB** (run 1: 12.5), free min 56.6 KB
  (run 1: 40.9). At the low point the tables held: scan 22.9 KB (162 rows), BLE
  23.4 KB (96 devices), baseline 12.4 KB, contacts 7.2 KB, anti 4.2 KB — the cached
  tables from finished modes are now the deck's main heap user, by design ("last look").
- **Open:** if more headroom is needed, the next lever is releasing or capping finished
  modes' cached tables (BLE strings: 96 × 17-char MACs are heap strings). The probe's
  9 KB low-water during the sniffer phase is still open. Output:
  `output/deck-stability/20261124-203704/` and `20261124-224155/` (git-ignored).

## 2026-11-24 — 2 h probe soak; combined deck+probe run finds a deck out-of-memory abort

**Phase:** stability · **By:** Claude + Will

- **Probe soak** (`bench-cc1101-driver/soak.py`, USB bench build of `7df8c65`, 2.0 h, all
  three lanes, Wi-Fi/BLE rotation every 15 min, Flipper every 30 min): 0 anomalies,
  0 OOK overflows/drops/errors, 148 OOK packets (144 LaCrosse ≈ one per 50 s, as
  expected), Flipper 4/4 at 0.22–0.26 s, sampled heap 79,872 → 79,348 B (−194 B/h).
  **Open:** the since-boot low-water mark ratcheted down during the sniffer phase
  (77 KB → 42 KB → 20 KB → **9,076 B** at 1.1 h), not at the scan/BLE rotations. Sampled
  free heap never moved, so these are short bursts. Hypothesis, not proven: Wi-Fi
  dynamic RX buffers (`DYNAMIC_RX_BUFFER_NUM` 32, internal RAM) filling under sniffer
  traffic bursts. rtl_433 stopped after 4 readings, so it gave no cross-check this run.
- **Combined run** (`tools/deck_stability.py`, probe on `build-uart`): smoke run of
  4 + 3 + 1 minutes. Script fixes found on the way: the deck reboots when its port opens
  (startup hold now tolerates boot output), LoRa needs `lora config` after a deck boot,
  and leaving a Wi-Fi/BLE card stops the PHY lane by design (the tour now restarts the mode).
- **Deck abort at 6 min:** `abort()` ← `std::bad_alloc` in
  `BtModel::operator=` ← `tickExternalGpsDisplay` (`external_gps_display.cpp:323`, the
  external panel's per-frame model copy). Deck free heap swung 137 KB → 56 KB with the
  Wi-Fi scan rows (~150 rows ≈ 40 KB), and the largest block fell 115 KB → 22 KB as views
  changed: fragmentation from frame copies being freed and reallocated per view switch.
  The deck has no PSRAM. Not OOK-specific; the BLE-live slot after a sniffer slot was the
  first copy that didn't fit. Fix not yet chosen.

## 2026-11-24 — GNSS card in the rotation; external OOK view

**Phase:** deck screens · **By:** Claude + Will

- **GNSS card** (Drive section, before WARDRIVE): the fix on its own — state, UTC,
  lat/lon, altitude, HDOP tier, fix age — with the external panel showing the fix
  reticle. Will found that reticle by accident: it is the external panel's fallback,
  and OOK RX was the last card without its own external view. Enter continues to
  Wardrive; the Link screen's GNSS row now opens this card instead of Wardrive.
- **External OOK view** ("433 MHZ // OOK FIELD"): one lane per transmitter with a
  ten-minute heard-when timeline drawn only from packets the deck still holds, RSSI per
  lane, and the selected transmitter in the dock (LaCrosse: temperature, humidity, last
  heard; remotes: count, RSSI, last heard). Badge goes FAULT/LOSS from `ook_status`.
- `model::ookSameTransmitter` is now public so the view and the device table share one
  identity rule.
- **Hardware:** deck `50:78:7D:CE:6D:64` flashed; `card gnss` and `card ook` both drive
  the panel (OOK at the 50 ms data cadence, cycle 46 ms); live LaCrosse 192/0 on screen.
- `sd-display-check` already failed to link before this (wifi/bt/contact detail,
  hunt, session strips missing from its source list); the OOK sources were added to it
  but the env still needs those others.

## 2026-11-24 — CC1101 work merged into main; probe and deck flashed

**Phase:** D-20 integration · **By:** Claude + Will

- `main` fast-forwarded to `cc1101-idf` (`0225962`); main had no commits of its own since
  the branch point. Will's uncommitted deck work (audio cues, key guide, idle sleep) was
  stashed, main moved, and the work reapplied; only the two worklogs and the renderer's
  source list overlapped, and each kept both sides.
- Old `firmware-c5/build*` folders were configured for ESP-IDF v5.5.1 and refused to
  reconfigure under v5.5.6; deleted, rebuilt clean. `check_protocol.sh` and
  `build_firmware.sh` pass on the combined tree.
- **Flashed:** probe `38:44:BE:BF:D2:94` (`build-uart`), deck `50:78:7D:CE:6D:64`.
  Deck debug console: OOK RX card, `ook` → probe `ook=rx`; after ~2 min the deck held
  1 device (LaCrosse 192/0), 3 packets, 0 link errors/timeouts.
- Deck `dump` now reports `ook_active`, `ook_devices`, `ook_packets`, `ook_unknown`.
- **Soak cut short:** the all-lanes soak (`output/soak/20261124-125229/`) stopped
  logging at 13:37 after 0.74 h; the script's process was gone, not the probe (no reboot,
  0 anomalies). Clean while it ran: heap 76,032 → 76,036 B (+7 B/h), OOK 56 packets,
  0 overflows/drops/errors, Flipper 2/2. Not a long-duration result; rerun it overnight.

## 2026-11-24 — Deck OOK RX screen; overnight all-lanes soak started

**Phase:** D-20 deck surface, branch `cc1101-idf` · **By:** Claude + Will

- **Soak:** started at 12:52 for 10 h on the bench probe (USB bench build
  at `a829110`), output in `output/soak/20261124-125229/` (git-ignored).
  - All three lanes run throughout: Wi-Fi sniffer, LoRa RX and OOK.
  - Every 15 min the PHY lane rotates through a full scan and a 60 s BLE
    scan.
  - The Flipper sends `A5C3E1` every 30 min, and rtl_433 runs alongside.
  - A 5-minute dry run was clean: scans of 100–114 networks, BLE 84–89
    events per minute, the Flipper burst decoded in 0.30 s, LaCrosse 6/6
    against rtl_433, 0 anomalies.
  - The deck can't reach the probe until the soak ends, because the bench
    build speaks OCP over USB.
- **Deck model `model/ook_model`:**
  - It folds `kind=ook` into a device table: one row per Princeton code,
    or per LaCrosse id + channel. Newest activity first, with the latest
    reading, a count and the first-heard time. A 16-row cap evicts the
    least recently heard.
  - It also keeps a 32-entry packet log, `ook_unknown` covered counts with
    the latest summary shape, and `[OOK]` reply handling: frequency and
    baud from every reply, health only from a complete counter set.
  - Every field is validated before anything is stored; unknown decoders
    are skipped.
- **`ui/ook_view` (home card OOK RX, Observe, after LORA RX):**
  - Rows show `PRN <CODE> xN` or `LAX id/ch temp hum [LOW]`, coloured
    RSSI and age, with a detail line for the selected device.
  - The hint row shows RADIO FAULT on `hw_fault`, or a LOSS line on
    overflow, drop, error or `ocp_drop`.
  - Keys: `s` start/stop, `; .` select.
- **`deck_app` wiring:**
  - `ook_listen_pending_` follows the same rule as LoRa: the `[OOK]` or
    error reply decides whether a session started.
  - `stop ook` and `stop all` clear the session; any error clears a
    pending listen.
  - Health is polled every 10 s while the screen is shown.
  - `reconcileOok()` reads `ook=` from status. It clears a session the
    probe lost; it adopts one the probe runs but the deck didn't start
    (cheaper than a second queued stop). Our own pending listen is never
    overridden.
  - LINK shows the `OOK` cap. The debug snapshot gained `probe_ook`, and
    there is a debug console `ook` command.
- **Tests:** `ook_model_test` 22/22, `link_reconcile` +8 asserts,
  `navigation_test` 7/7.
- **Mutation proofs, 7/7 caught:** LaCrosse identity ignoring the channel;
  a partial health set accepted; no move-to-front; uppercase hex accepted;
  `n=0` unknown accepted; a bad RSSI defaulting to zero; reconcile ignoring
  a pending listen.
- **Rendered on the host** (`tools/render_external_views.py`, fixtures
  `deck-ook`, `deck-ook-sensor`, `deck-ook-idle`). The first render showed
  wrong ages: the fixture clock was 30 s, so hour-old timestamps wrapped.
  The fix was a fixture-only clock base; the view itself was right.
- **Builds:** both firmwares, deck RAM 22.9%, flash 29.1%. Host suite
  green.
- **Not yet:** a deck-on-hardware session (after the soak), SD logging of
  OOK sensors, and an external-TFT OOK view.

---

## 2026-11-24 — Heap pass 2: queues and NimBLE to PSRAM, stacks sized from measurements

**Phase:** D-20 hardening, branch `cc1101-idf` · **By:** Claude + Will

**Method.** Scratch builds only, never committed. A monitor task logged
every task's minimum stack headroom
(`uxTaskGetSystemState`, trace facility on), plus free internal heap
after each boot init step. One boot covered:
- `--gate-wifi`, `--gate-sniffer`, `--gate-ook` and `--gate-stop`;
- every engine: `scan_bt`, BLE scan, airtag, antisurveillance,
  `hunt_ble`, `hunt_wifi`, `inspect_network`, `start_wifi_scan`, the
  sniffer and its tables, deauth, `channel_view`, `packet_monitor`,
  `lora_sweep`, wardrive with BLE concurrent with LoRa RX and OOK, the
  `set_*` NVS writes, and `zig_recon` with its tables, run last because
  stopping it restarts the probe by design.

**Boot cost of internal RAM, before this pass:**

| Init step | Cost |
|---|---|
| Wi-Fi | 53.9 KB |
| NimBLE + controller | 31.6 KB |
| OCP (transport, frame, server) | 20.5 KB |
| LoRa + OOK | 12.1 KB |
| 802.15.4 | 6.3 KB |
| NVS + LED + arbiter | 4.9 KB |

**Findings:**
- **`sys_evt` stack:** 2,304 bytes with only **224 bytes free**. That task
  runs our Wi-Fi scan-done handler.
- **Oversized stacks:**

  | Task | Stack | Peak use |
  |---|---|---|
  | `led` | 2048 | 268 |
  | `wifi_net_evt` | 3072 | 640 |
  | `cc1101_cap` | 3072 | 892 |
  | `zig` | 4096 | 748 |
  | `lora_sweep` | 4096 | 832 |
  | `ook` | 4096 | 1136 |
  | `lora_radio` | 4096 | 304 |
  | `lora_drain` | 4096 | 588 |

  The two LoRa receive stacks are unchanged: `rx=0` during the run, so
  their receive path was never exercised.
- **Tight but left alone:** `ocp` 796 free, `ocp_evt` 876, `nimble_host`
  772.
- **NimBLE** compiles in central, peripheral, broadcaster, GATT
  client/server, SMP, privacy and 12 demo GATT services. The probe calls
  only `ble_gap_disc*`.

**Changes:**
- **OCP event queue** (32 × 268 B = 8.6 KB) and **Wi-Fi discovery queue**
  (3.6 KB) now in PSRAM (`xQueueCreateWithCaps`), with internal RAM as
  the fallback. Every producer is a task, never an ISR: the Wi-Fi task's
  promiscuous callback, the NimBLE host, and the engines.
- **NimBLE host pools in PSRAM**
  (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`): BLE's boot cost drops from
  31.6 KB to 26.9 KB. Scans were unchanged: `scan_bt` 75–77 devices,
  continuous scan 86 events in 20 s, against 89 before.
- **Stacks:** `sys_evt` 2304 → 3584. `led` 1280, `wifi_net_evt` 2048,
  `cc1101_cap` 2048, `zig` 2560, `lora_sweep` 2560, `ook` 2560.
  Re-measured headroom: sys_evt 1504, led 1012, wifi_net_evt 1392,
  cc1101_cap 1192, zig 1808, lora_sweep 1740, ook 1428.

**Results, real bench build (free internal heap, bytes):**

| State | `main` | Before pass 1 | After pass 1 | Now |
|---|---|---|---|---|
| Idle | 77,116 | 63,180 | 67,068 | **83,748** |
| Lora + OOK | – | 54,300 | 58,188 | **76,404** |
| Sniffer + LoRa (+ OOK), 30 s | 72,432 | 54,056 | 57,944 | **76,164** |

- Largest free block at idle: 61,440 bytes, up from 40,960.
- The worst low-water mark in the full exercise is now 40,716. It is set
  by the Wi-Fi driver's own allocations during `scan_networks`,
  `hunt_wifi` and `inspect_network`, which this pass leaves alone.
- One-off: on the first Wi-Fi start after a flash, the low-water mark
  dipped to 43,240. After normal reboots it stays at 71,592–72,620.
  Probably the radio's full first-time calibration (not proven); start
  soak measurements after a reboot.

**Performance:** OOK over OCP with all three lanes live: LaCrosse equal to
rtl_433; Flipper 4/4 at 0.17–0.18 s latency (0.17–0.19 s before); 0 false
or unknown packets; 0 overflows, drops or errors. BLE scan counts
unchanged. The event queue copy from PSRAM is ~268 B per event, against a
115200-baud link.

**Verified:** host suite and receive-only audit green.
- Real build: `--gate` 17/17 (+1 skip), `--gate-stop` 15/15.
- Instrumented build: `--gate-wifi` 41 (+1 skip), `--gate-sniffer` 23,
  `--gate-ook` 22. No errors and no unplanned reboots in the exercise.

**Open:**
- **Observer-only NimBLE.** It frees +3.9 KB internal and 112 KB of flash,
  and would make receive-only structural. But on v5.5.6 discovery starts
  and then delivers no reports and no completion. The host and controller
  config diff is host-only; not yet root-caused.
- **LoRa receive-path stacks:** need a LoRa source (the LoRaTrace
  Heltec transmitter bench) before they are resized.

---

## 2026-11-24 — Heap pass 1: reply frames and the CC1101 buffer move to PSRAM

**Phase:** D-20 hardening, branch `cc1101-idf` · **By:** Claude + Will

Measured on the bench probe, both builds on v5.5.6. Against `main`, the
OOK lane cost 13.9 KB of free internal heap at idle:
- 5.8 KB static: FreeRTOS stream-buffer code in IRAM (+2.8 KB), other
  IRAM (+1.4 KB), and `.bss` (+1.6 KB).
- ~8 KB allocated at boot: the capture task's 3 KB stack and its 4 KB
  message buffer.
- Running OOK adds a 4 KB engine stack.

After a Wi-Fi scan, the largest free internal block was 32,768 bytes,
against ~45 KB on `main`. Block frames are built by doubling a buffer up
to 256 rows (~66 KB). A full page would then need 32 KB of contiguous
internal RAM and could be dropped silently.

- **`ocp_block.c`:** on the probe, frames are built in PSRAM, with
  internal RAM as the fallback. The host build keeps `realloc`.
  - Proved with a throwaway build that logged `esp_ptr_external_ram()`:
    every allocation from 1 KB to 16 KB landed in PSRAM, and a
    130-network scan arrived whole.
  - The first throwaway build used `printf`, which overflowed the system
    event task's 2,304-byte stack (`rst=panic`). An instrumentation
    artifact, but it shows how tight that stack is; see pass 2.
- **`cc1101_capture.c`:** the 4 KB message buffer moved to PSRAM
  (`xMessageBufferCreateWithCaps`), with internal RAM as the fallback.
  Only tasks touch it.
- **Result (free internal heap, bytes):**

  | State | Before | After |
  |---|---|---|
  | Idle | 63,180 | 67,068 |
  | All three lanes live, 30 s | 54,056 | 57,944 |
  | Low-water mark after a scan | 49,532 | 51,396 |

  `main` idles at 77,116.
- **Gates:** `--gate-wifi` 41 passed (+1 skip), `--gate-ook` 22/22,
  `--gate-stop` 15/15. Host suite green.

---

## 2026-11-24 — ESP-IDF v5.5.6; USB replies over 1 KB were dropped (fixed)

**Phase:** tooling, branch `cc1101-idf` · **By:** Claude + Will

- **Toolchain:** the shared `~/esp/esp-idf` moved v5.5.1 → v5.5.6 in
  place.
  - The patch was reverse-applied first, so the tree was clean.
  - The spiffs submodule needed a second `submodule update`.
  - `install.sh esp32c5,esp32s3`: the first run failed upgrading pip; the
    second succeeded.
  - It left a half-removed esptool. pip listed 4.14.0, but 4.12.0 code
    loaded. Force-reinstalled under ESP-IDF's constraint file.
  - Now: GCC esp-14.2.0_20260121, esptool 4.14.0.
- **Repo:** pin 5.5.6 in `build_firmware.sh` and CI. Removed the vendored
  patch, the hash check and the `-dirty` allowance. Updated
  BUILD_REPRODUCIBILITY, AGENTS (install line, gotcha 10), README.
  Historical 5.5.1 mentions (CHANGELOG, audits, fuzz sample) left as they
  were.
- **Verified on v5.5.6:**
  - Clean UART and bench builds, 0 warnings; the image reports `v5.5.6`.
  - `check_protocol.sh` green.
  - `--gate` 17/17 (+1 skip), `--gate-stop` 15/15, `--gate-ook` 22/22.
  - Flashing over USB with no BOOT button, from v5.5.6 firmware.
- **`--gate-wifi` fails, and it is not the toolchain.** The failing check
  is "scan_networks replies with [SCAN]"; the other 8 pass.
  - Both controls fail the same way: `main` 572a512 built on v5.5.6, and
    the 21 Nov v5.5.1 bench image from the main worktree.
  - Status polling shows `owner=wifi` until ~11 s, then `owner=none`. The
    scan completes, but no `[SCAN]` block ever arrives, even after 150 s.
    `stop` mid-scan still gets its aborted frame.
  - **Cause, found and fixed the same day:** the USB transport handed a
    whole block frame to `usb_serial_jtag_write_bytes`. That driver queues
    each write as one byte-ring-buffer item, and the ring buffer refuses
    outright any item larger than its 1024-byte size, with no partial
    write. So every reply over 1 KB was dropped silently on USB.
    - It began on 2026-09-17 (06e2427), when replies became
      transactional blocks; `--gate-wifi` last passed on 2026-09-15.
    - Grove UART is unaffected: `uart_write_bytes` feeds its buffer
      incrementally.
    - Evidence: a 1-channel scan (8 networks, 758 bytes) arrived; a
      3-channel scan arrived as fragments only.
  - **Fix:** `ocp_transport_write` (USB) now sends 512-byte chunks, each
    with the same 100 ms bound, failing on the first short chunk.
    - `--gate-wifi` 41 passed, 1 skipped (a full scan of 128 networks),
      matching the September result.
    - The unfixed build failed that check every run, which is the
      proof that the gate catches it.
    - `--gate` 17/17, `--gate-stop` 15/15 and `--gate-ook` 22/22 on
      the fixed build.
- **Flashing:** reflashing from the v5.5.1 control image parked the C5 in
  ROM again, as expected, since that image still has the bug.

---

## 2026-11-24 — C5 flashing fixed: ESP-IDF IDFGH-17050, not our firmware

**Phase:** tooling, branch `cc1101-idf` · **By:** Claude + Will

`flash_c5.sh` kept failing with "Write timeout", leaving the C5 parked in
ROM at `0x4003B10E`. Will had to hold BOOT at power-on to recover, and has
hit the same thing on other C5 projects. The cause is ESP-IDF bug
IDFGH-17050 (esp-idf#18089). The conditions are a C5 rev v1.0 (this one),
the USB Serial/JTAG console (both our builds), and a power-on-type reset
(power cycle or RTS hard reset). Under them, v5.5.1's `esp_perip_clk_init`
gates UART0's clock, and the ROM then loops when esptool resets the chip.
After a watchdog or software reset the gating is skipped. That is why
flashing only failed sometimes.

- **Fix:** Espressif's v5.5 backport `ffc08c2`, vendored unchanged as
  `tools/idf-patches/v5.5.1-esp32c5-usb-reset-IDFGH-17050.patch` and
  applied to the local ESP-IDF. Upstream it is in v5.5.5, v5.5.6, v6.0.3
  and v6.1+.
- **Build check:** `build_firmware.sh` now requires the patched `clk.c`,
  matched by git blob hash, and nothing else changed in the ESP-IDF tree.
  It then accepts `v5.5.1-dirty`.
  - Proof in a scratch repo: the unpatched file and the patch plus a
    stray edit are both refused; only the exact patched file is accepted.
  - The first version, `apply --reverse --check`, let the stray edit
    through.
- **CI:** applies the patch after cloning.
- **Hardware:** before the patch, 4 of 5 flashes failed: every one tried
  while this branch's firmware was running, which had last come up from an
  RTS or power-on reset. The one success was from a watchdog-reset boot.
  After the patch, 5 back-to-back `flash_c5.sh` runs succeeded, each
  following the previous run's RTS hard reset. A power-cycle followed by a
  flash, the strongest trigger, is not yet re-tested.
- **Earlier wrong guess:** before finding the bug report, I suspected the
  CC1101 boot code. That was wrong; `main`'s firmware has the same console
  configuration.
- **Next:** ROADMAP item to move to v5.5.6 and drop the patch.

---

## 2026-11-24 — OOK lane over OCP: CC1101 is the third concurrent radio (D-20)

**Phase:** D-15 → D-20, branch `cc1101-idf` · **By:** Claude + Will

The CC1101 is now an OCP lane alongside the PHY lane (Wi-Fi/BLE/802.15.4)
and the LoRa lane. Will chose the design: a concurrent lane of its own, named
`ook`, with rate-limited summaries for undecoded signals. That gives cap
`ook_rx`, `ook_listen [freq_khz]`, `ook_status`, `stop ook`,
`[STATUS] ook=absent|idle|rx`, and `kind=ook` / `kind=ook_unknown` events
(OCP-SPEC §15, §5.4).

- **`ook_recon.c`:** probes the chip at boot and parks it in SLEEP. A
  missing chip leaves no cap and reports `ook=absent`. Each listen
  re-applies the profile, because SLEEP loses 0x29–0x2E (SWRS061I §29.2).
  An engine task at priority 4 feeds capture chunks through pulse →
  decode → events. A 1 s watchdog verifies the profile and MARCSTATE and
  re-inits on drift. `probe_restart` won't restart while OOK runs.
- **`ook_events.c`:** identical packets within 1.5 s merge, and each repeat
  extends the window. Eight recent packets are held and the least recent
  is evicted. Unknown trains produce at most one summary per second,
  carrying the count of trains it covers.
- **Capture chunks carry RSSI** (`rssi=` on events, the peak while the
  train was sampled).
- **Host:** `ook_test` 60/60, up from 45. Mutation proofs on `ook_events`:
  10/10 caught. The first pass let "evict the newest" survive, so an
  eviction-order test was added. Conformance 27/27; receive-only audit
  clean.
- **Hardware (C5, USB-OCP bench build):**
  - `ocp_repl.py --gate-ook` (new, passive): 22/22. The sniffer, LoRa RX
    and OOK ran at once, and each scoped stop spared the other two lanes.
  - A mutant firmware where `stop phy` also stopped OOK, built from a
    scratch copy: 3 checks failed.
  - `bench-cc1101-driver/ocp_xref.py` (new; needs `--transmit` for the
    Flipper), 330 s with all three lanes live:
    - LaCrosse readings identical to rtl_433: 6 events. rtl_433 reported
      12 because each reading is 2 trains, which the lane merges.
    - Flipper: 4 bursts gave 4 events, latency 0.17–0.19 s.
    - 0 false or unknown packets, and 0 overflows, drops, errors or
      re-inits.
  - `--gate` 17/17 (+1 skip, 3 runs) and `--gate-stop` 15/15.
- **The earlier `--gate` reboot failure did not reproduce.** `main`'s
  firmware (572a512) passed, and so did this branch, 3 times. Rebooting
  with OOK active came back on the same port, idle. A different failure
  shows up if a lane is left running before the gate starts: "stop is
  acked when idle" sees `running=1`. That is the gate's idle assumption,
  and a live LoRa session trips it the same way.
- **Flashing:** twice today `flash_c5.sh`'s `usb_reset` handshake left the
  C5 parked in ROM (PC 0x4003B10E, gotcha 10), with "Write timeout" on both
  attempts. Pressing RESET/BOOT and flashing with `--loader` recovered it.
- **Still open:** the deck view and the deck's OCP client (`ook=`,
  `kind=ook`); JTAG attach.

---

## 2026-11-23 — CC1101 OOK decoder: Princeton and LaCrosse TX141THBv2, live on the C5

**Phase:** D-15, branch `cc1101-idf` · **By:** Claude + Will

Two pure-C modules sit on top of `cc1101_capture`. `ook_pulse.c` turns
the 25 kbaud sample stream into pulse trains: chunk-boundary independent,
one-sample glitches absorbed, a train closed as soon as silence reaches
6 ms, and a cut at 512 entries flagged. `ook_decode.c` decodes those trains:

- **Princeton/EV1527:** 24 bits, 1:3 PWM, te 80–700 µs. Each frame is
  closed by a 1te sync pulse and a ≥8te low.
- **LaCrosse TX141THBv2:** ≥3 sync pairs, then 40 PWM bits. The fields are
  ID, battery-low, test, channel, temperature (raw − 500, in 0.1 °C) and
  humidity. An 8-bit reflected LFSR digest (generator 0x31, key 0xF4) is
  checked against byte 5. Bench frame `C0 82 51 38 → 11` matched rtl_433's
  192 / 9.3 °C / 56 %.

Identical frames in one train merge, so `frames` counts repeats. The bench
console prints an `O,` line for each decoded packet. It is not `P,`, which
the S3 harness already uses for power-cycle reports. `raw off` hides the
`B,` lines.

- **Frame layout came from the bench capture,** not from another decoder.
  4×(4 sync pairs + 40 bits + a trailing pulse), and the sensor sends two
  such bursts per reading.
- **Replay of a 5.5-minute capture through the C code:** LaCrosse 14/14
  against rtl_433, with every field equal, including the 9.3 → 9.2 °C
  change. Flipper `A5C3E1` gave 40/40 frames at te 398–399 µs. All 54
  trains were real signals; there were no noise trains.
- **Live on the C5, SX1262 sweeping, 5.5 min:** LaCrosse 14/14 against
  rtl_433 and Flipper 40/40. There were 0 unexpected packets, 0 overflows,
  0 dropped chunks, and 0 errors in 30,526 sweeps. A 110-second rerun after
  the `O,` rename gave 4/4 and 40/40.
- **The host test (`ook_test`, 45 checks, ASan/UBSan) found a real bug.**
  With a 2.0 long/short floor, Princeton decoded LaCrosse bits (~1:2 PWM) as
  bogus codes. The floor is now 2.4; Princeton is nominally 1:3 and the
  Flipper measured 3.0.
  - The other cases are random chunking, CC1101-like skew, glitches, all 40
    single-bit LaCrosse corruptions, clipped or cut frames, 200 s of random
    sample noise (0 packets), 2,000 hostile trains, and capacity limits.
- **Mutation proofs:** all 10 caught. The first pass let three through: the
  LaCrosse end-of-train bound, the Princeton sync check, and a spike out of
  silence. Each exposed a weak test, and the tests were strengthened until
  all three fail (ASan overflow, a 5te sync, a post-gap spike).
- **Still open:** OCP verbs, the sub-GHz arbiter, and the deck view.

## 2026-11-23 — CC1101 capture task: lossless 25 kbaud bitstream on the C5

**Phase:** D-15, branch `cc1101-idf` · **By:** Claude + Will

This closes the overflow item from the entry below. `cc1101_capture.c` drains
the RX FIFO from its own task, at priority 6 (below OCP dispatch, above the
engines). It runs on an 8 ms `xTaskDelayUntil` tick and sends timestamped
chunks to a 4 KB message buffer. A slow consumer loses chunks there, counted
as `dropped`; the chip never overflows because of it. Other sequences on the
chip, such as apply, reset, stress and the drift check, run inside
`cc1101_capture_pause()`/`resume()`.

- **Bug found and fixed in `cc1101_port.c`:** the "lock held" state was one
  flag shared by every task. When the bench task held the bus, the capture
  task saw `locked`, skipped the lock, and started a concurrent polling
  transaction. It then released the bench task's mutex. The result was an
  IDF `spi_device_polling_start ... previous polling transaction is not
  terminated`, then `assert failed: xTaskPriorityDisinherit`, then a reboot,
  reproduced on the first FIFO start. The holder is now tracked per task, and
  CSn, clocking and the lock are only touched by the holder. The bug was
  there from the start; it was hidden because only one task had ever used
  the CC1101.
- **Second fix:** `cc1101_capture_start` restarts RX with an empty FIFO.
  Before this, a FIFO left overflowed by a stopped capture was counted as the
  new capture's first overflow (1 seen after a 25 → 8 ms switch; 0 after the
  fix).
- **LaCrosse vs `rtl_433`, 5 min, SX1262 sweeping:** 6/6 transmissions with
  the same run structure as before. 943,526 FIFO bytes (the full 3,125 B/s),
  **0 overflows** (14 before), 0 dropped, 0 late ticks. 36,147 sweeps,
  0 errors.
- **Flipper Princeton (`fifo_runs.py`):** 5/5 with and without load, runs at
  10 and 30 bits, 0 overflows, 0 dropped, 0 SX1262 errors.
- **The check goes red:** with the SX1262 sweeping, a 25 ms drain tick (longer
  than the ~20 ms the FIFO lasts) overflowed on almost every tick (~40/s),
  with 8 bytes captured. 18 ms gave 0 overflows in 20 s, and 8 ms gave 0 in
  60 s after deliberate starvation. The 8 ms default has about 2.5× margin.
  The bench drift watchdog flagged the starved chip as `marc=11`
  (RXFIFO_OVERFLOW), as intended; it saw no drift at 8 ms.
- Bench: `drain N` now accepts 1–40 ms and restarts the capture. `I` lines
  add `fifo_dropped`, `fifo_late` and `drain_ms`, and the console line buffer
  grew to 480 bytes (the longer `I` line had been truncated). `fifo_runs.py`
  waits for the C5 to boot, because opening the USB console sometimes resets
  it.
- **Still open:** pulse decoder, OCP verbs, sub-GHz arbiter, deck view.

## 2026-11-23 — CC1101 bitstream over SPI (no GDO0 wire): qualified on the C5

**Phase:** D-15, branch `cc1101-idf` · **By:** Claude + Will

Will preferred a clean drop-in module over soldering GDO0 to a XIAO back
pad. Seeed's schematic shows TP4–TP7 = MTDO/MTCK/MTDI/MTMS = GPIO5/4/3/2;
GPIO4 was the candidate if a wire is ever needed. Rejected for now. The
CC1101 now samples OOK into its 64-byte RX FIFO at 25 kbaud (normal FIFO,
infinite length, no sync), and the C5 drains it over the shared SPI bus,
leaving the last byte in place (SWRS061I §20).

- **Flipper Princeton (te 400 µs), 5/5, with and without SX1262 load:**
  runs of 10 and 30 bits (te and 3te) within ±1. Quiet noise gave zero
  3te-like runs. 3,012 SX1262 sweeps, 0 errors.
- **LaCrosse-TX141THBv2 vs `rtl_433`, 5 min:** all 6 transmissions show the
  same pulse structure. High runs are 5–6/11–12/17–18 bits (~220/460/720 µs)
  and low runs 7/12–13/19 bits. The short pulse matches the 232 µs measured
  by SDR in September. Quiet time had 15 short runs out of 1,044.
- **Sensitivity:** FIFO reads cost ≤0.2 dB against the async profile at the
  same data rate (12.5k: −88.7/−88.5; 25k: −87.2/−87.5 dBm). Weaker SPI
  drive and a 16 ms drain made no difference. The reported floor follows
  the configured data rate in both modes (5k: −89.6 dBm), so an earlier
  "3 dB SPI desense" reading was wrong. True sensitivity versus data rate
  needs an attenuated source.
- **Open:** about 1 RX FIFO overflow per 20 s at 25 kbaud (14 in 5 min) when
  draining from the bench console loop. Recovery works (flush, recalibrate,
  RX), but each overflow loses ~20 ms. The product needs a dedicated
  higher-priority drain task on a fixed tick. A pulse decoder, OCP verbs, the
  arbiter and a deck view are also still to do.

## 2026-11-23 — CC1101 receives on the C5 backpack; shared bus holds under load

**Phase:** D-15 harness validation, branch `cc1101-idf` · **By:** Claude + Will

Hardware: the blue CC1101 wired to the probe per Rev D §4.4 (CSn D3/GPIO7
with a 10 kΩ pull-up to 3V3, 100 nF + 10 µF, its own 433 MHz antenna,
GDO0/GDO2 open), sharing D8/D9/D10 with the Wio. Firmware: `--cc1101-bench`
build. A clean bring-up was confirmed after every flash.

| Check | Result |
|---|---|
| Bring-up | probe/reset ok, SO edge seen, 44/44 registers verified, 0 retries, RX confirmed, calibrated. Floor −89 dBm (S3 bench: −86) |
| Wio under the new bus lock | 1,170 → 30,285 SX1262 receive-only sweeps, 0 errors; XOSC_START_ERR cleared at cold start as usual |
| Coexistence (`coexist.py --stress`) | CC1101 floor −89 dBm with the SX1262 idle, sweeping and idle again; Flipper 5/5 in each phase; 500 bring-ups and 2,000 RX cycles under load with 0 failures; 0 drift |
| LaCrosse-TX141THBv2 vs `rtl_433`, 5 min, SX1262 sweeping throughout | 6/6, +22..+30 dB, 104/104 raised windows within ±2 s of a decode, 0 strays |
| Boot / USB recovery with the pull-up | normal boot; USB flash from the running app 2/2 |

**Wiring lesson:** the first pass put the 10 kΩ in series between D3 and
CSn, so there was no pull-up. During that time, flashing from the running
app failed four times. Two attempts were with the deck unplugged, so the deck
was not the cause. The errors were "TX path down" and "serial data stream
stopped". One flash succeeded from a manual BOOT+RESET loader. After the
pull-up was rewired, flashing from the running app went 2/2. The floating
GPIO7 (JTAG-source strap) correlates with the failures, but the mechanism
is not proven.

The ~1% of `coexist.py` windows that rose with no Flipper source were the
LaCrosse. The SDR run accounted for every raised window, and the rate did
not rise with SX1262 load.

**Still open:** deck reconnected — OCP over Grove and a LoRa receive session
on the new firmware, plus a USB flash with the deck attached; JTAG attach
with the pull-up; a GDO0 pin decision; the OCP verbs, arbiter and deck view.

## 2026-11-23 — D-15 reopened: CC1101 on the C5 shared bus, bench console

**Phase:** D-15, branch `cc1101-idf` · **By:** Claude + Will

Will reopened D-15 and approved CC1101 CSn on C5 GPIO7/D3. Rev D §4.4 now
holds the pins and parts. `docs/hardware/c5-dual-radio-wiring.md` is
restored as the rationale. GDO0/GDO2 have no pin, so the receiver is
RSSI-only.

- `subghz_spi.c` now owns the SX1262/CC1101 bus pins, the idempotent bus
  init, and one mutex. Each driver holds that mutex for its whole
  chip-select-low span. Before this, `lora_radio.c` drove NSS around
  `spi_device_transmit` with no lock. A second device's CS-low window could
  then have clocked commands into the wrong chip. `lora_radio.c` now takes
  the lock in `cs_select()` and fails with `ESP_ERR_TIMEOUT` after
  `SUBGHZ_SPI_LOCK_TIMEOUT_MS` instead of colliding.
- `cc1101_port.c` is the ESP-IDF transport for `cc1101_bus`. It refuses to
  clock unless it holds the lock, and reads SO as a GPIO for CHIP_RDYn.
- `cc1101_bench.c` is behind `CONFIG_OSCILLA_CC1101_BENCH`, built with
  `build_firmware.sh --cc1101-bench`. It serves the `bench-cc1101-driver`
  line console on USB while OCP stays on Grove, so the S3 scripts run
  unchanged against the C5. `lload on|off` runs a continuous SX1262
  receive-only RSSI sweep (902–928 MHz) as the coexistence load. It adds no
  OCP verb.

**Verified (host and build only):** both firmwares build. The normal probe
build is checked to exclude the bench console, and the `--cc1101-bench`
build has it with UART transport. `check_protocol.sh` passes, including
`cc1101_test` 348/348. `check_rx_only.py` and `ocp_repl.py --selftest`
(27/27) pass. **Not yet on hardware:** C5 bring-up, the Wio regression
under the new lock, and coexistence.

## 2026-11-23 — CC1101 driver stress, soak and power-cycle qualification (blue module)

**Phase:** D-15 requalification, branch `cc1101-idf` · **By:** Claude + Will

Will asked for this before any dual-module harness work, since the Koko
had looked healthy before it failed. Hardware is the blue breakout
direct-wired to the XIAO S3, running `bench-cc1101-driver`. All results
below come from the probe's unchanged `cc1101_*.c` layers.

- **Bring-up cycling:** 200 + 5,000 full reset→apply→rx_start rounds, and
  500 + 20,000 rx stop/start pairs. 0 failures, the post-SRES SO edge was
  seen every time, 0 register retries, and the profile verified with
  MARCSTATE=RX at the end.
- **30-minute soak** with `rtl_433`, a Flipper burst every 30 s, and 1,000
  bring-ups at minutes 10 and 20. Flipper 60/60. LaCrosse 32/32 while
  receiving: the 33rd decode fell inside the deliberate 5.5 s cycling
  blackout. 0 drift events. Floor steady at −86 dBm. The two "stray"
  windows were the first windows after each cycling burst: GDO0 counts
  piled up from the POR clock briefly restored by each SRES.
- **Manual USB power cycles:** 2 of 2 passed, with 2.9 s and 2.1 s off. A
  clean cold-boot bring-up, a Flipper burst received within about 3 s of
  boot, and no drift.
- **Automated power cycling:** the CC1101 VCC was moved to S3 D4/GPIO5, with
  every CC1101 line pulled down while off. 1,052 cold starts across 10 ms to
  5 s off-times, both POR-only and with SRES: 0 bring-up failures, Flipper
  54/54 after cold starts, 0 drift. A second pass read FREQ2 before any
  write and confirmed a genuine power-on reset on 1,240/1,240 starts, down
  to 1 ms off.
- Before the supply was switched on at D4, the live drift watchdog reported
  `probe=no_chip` / `marc=00`. That confirmed the dead-bus detection on
  real hardware, and it recovered by itself once powered.

**Limit:** the GPIO pulls the rail down actively. A slow, partial
brown-out (a sagging rail that never reaches 0 V) is covered only by the
two manual unplugs, not by the automated run. **Reading:** the blue module
and the Oscilla driver are qualified on this bench for bring-up, sustained
reception and cold starts. The SX1262 shared-bus harness is next.

## 2026-11-23 — Oscilla CC1101 driver receives on hardware; A/B matches SmartRC

**Phase:** D-15 requalification, branch `cc1101-idf` · **By:** Claude + Will

Hardware: the blue CC1101 breakout from September's failed C5 runs, now
direct-wired to the spare XIAO S3 (no breadboard, no CSn pull-up), with
GDO0 on D1. Will retired the Koko after it failed from a clean power-on
under the known-good sketch (see `cc1101-bench` WORKLOG).

| Same module, same bench | SmartRC reference | Oscilla driver (`bench-cc1101-driver`) |
|---|---|---|
| Bring-up | ok | probe ok, reset ok (SO edge seen), 44/44 registers verified, 0 retries, RX confirmed, calibrated |
| Write sweeps | 42 × 256, 0 errors | apply read-back 44/44; drift verify clean throughout |
| Idle floor / GDO0 edges | −85 dBm / 0 | −86 dBm / 0 (with 200 µs RSSI spacing) |
| Flipper bursts (made-up Princeton key) | 5/5, +35..+37 dB, 26–27 edges | 5/5, +56..+58 dB, 26–27 edges |
| LaCrosse-TX141THBv2 vs `rtl_433` (5 min) | 6/6, +12..+17 dB, 55–56 edges, 0 strays | 6/6, +20..+25 dB, 55–56 edges, 0 strays |

The larger margins in the Oscilla column should not be read as a driver
gain. The SDR's own reading of the same sensor rose about 4 dB between
runs, and the Flipper peaks jumped about 20 dB with the code unchanged,
so the bench geometry most likely moved. Both drivers detect every
transmission, with the same GDO0 bitstream rates.

**Found on hardware:** back-to-back RSSI polling coupled into the receiver.
With SPI idle there were 0 false GDO0 edges. Continuous polling gave 2–8
edges per 20 ms window and raised the floor 4–6 dB. A 200 µs gap removed
both. This is now `CC1101_RSSI_POLL_MIN_US`. On the C5 the SX1262 shares
this bus, so the shared-bus duty cycle needs the same scrutiny. The live
registers under our driver matched SmartRC's except the FSCAL calibration
results.

**Reading:** the module and the 433 MHz chain work, and Oscilla's own
clean-room layers receive as well as the reference. Combined with
September's register record, the September null result points at the
archived C5 driver: no calibration, GDO0 clock left on, and no write
verification. A same-module C5 rerun of the old driver would confirm
it, but this was not run. **Still open:** an ESP-IDF `spi_master`
transport on the C5's shared bus with the SX1262, Rev D CC1101 pins (CSn
and GDO0), and reopening D-15, which is Will's call.

## 2026-11-23 — CC1101 receive start with guaranteed calibration

**Phase:** D-15 requalification, branch `cc1101-idf` · **By:** Claude + Will

Added `cc1101_rx.c`. It flushes the RX FIFO, then calibrates the
synthesizer, then sends SRX and confirms MARCSTATE=RX. When
MCSM0.FS_AUTOCAL does not cover IDLE->RX, it sends an explicit SCAL first.
That is the September failure: that driver left FS_AUTOCAL=0 and never
calibrated. It also converts RSSI to dBm per SWRS061I §17.3, rounding
toward −inf. **Verified:** `cc1101_test` 346/346 under ASan/UBSan. All 6
reintroduced bugs were caught, including dropping the SCAL, skipping the
RX confirmation, and signed or rounding RSSI errors. `check_protocol.sh`
and `check_rx_only.py` pass, and the file compiles for the C5. Not on
hardware yet.

## 2026-11-23 — Clean-room CC1101 receive layers (bus, profile, verified config)

**Phase:** D-15 requalification, branch `cc1101-idf` (not linked into the
probe build) · **By:** Claude + Will

Will chose a clean-room ESP-IDF CC1101 driver that matches the behaviour of
SmartRC-CC1101-Driver-Lib over a downstream fork. SmartRC has no standard
licence, and it re-initialises SPI on every access and waits without
bounds. On 2026-11-23 a module under SmartRC received 6 of 6 SDR-confirmed
and 5 of 5 Flipper transmissions (see the `cc1101-bench` WORKLOG). This
commit lands parts 1–3. Register facts come from TI SWRS061I, extracted
from the PDF, not from memory. No SmartRC code is used.

- `cc1101_profile.c`: the 47-register image read back from that receiving
  chip (433.92 MHz OOK async, 650 kHz, `MCSM0=0x18`), plus band-checked
  frequency helpers. FSTEST, PTEST and AGCTEST are never written.
- `cc1101_bus.c`: an injected transport, so the layer owns no pins (Rev D
  has no CC1101 since D-15 was archived). Every wait is bounded. Seven
  receive-side strobes are allowlisted; STX, SFSTXON, SFTX, PATABLE and the
  TX FIFO are unreachable. Reset accepts a missing post-SRES SO edge and is
  judged by POR FREQ2 plus IDLE. A probe rejects the all-zero dead bus that
  a passing CHIP_RDYn check hides. Status reads repeat until two agree
  (SWRS061I §10.3).
- `cc1101_config.c`: applies the profile in IDLE, GDO0/GDO2 first, with
  per-register read-back and bounded retries. Failures are named. A
  not-ready chip aborts immediately. A drift verify masks the FSCAL result
  fields, so a silent brown-out reset is detectable after autocal.
- `cc1101_sim.c` (host test only): a byte-level chip simulator with the
  bench faults: dead bus, SO stuck high, no SO edge after SRES, writes
  wedged after SRES, ignored SRES, dropped writes, MARCSTATE read flicker,
  and a mid-apply loss of the chip.

**Verified:** `cc1101_test` 325/325 under ASan/UBSan. Each of 13 bugs was
reintroduced on a copy and caught; the first sweep missed the mid-apply
abort, and a test was added for it. Full `check_protocol.sh` passes.
`check_rx_only.py` passes and now reserves the CC1101 transmit names. All
three files compile for the C5 with `riscv32-esp-elf-gcc` 14.2.0 `-Wall
-Wextra -Werror -Wshadow -Wconversion` (~2.2 KB text). **Not done:**
RX start with confirmed calibration and MARCSTATE=RX, the ESP-IDF
`spi_master` transport on the shared bus, Rev D pin allocation, and
any hardware run. D-15 stays Archived.

## 2026-11-23 — Contextual deck key guide

- Replaced the shared `h` screen with controls for the current card or
  nested view. The footer exposes `h HELP` and Wi-Fi Scan, BLE Scan,
  and Anti-Surv advertise `f HUNT`. Help now blocks the Packet Monitor
  Enter action while open. The companion TFT continues showing its live view.
- Added [Deck Controls](docs/deck-controls.md) for the longer workflows.
  Verified four key-guide renders at 240×135 and generated 175 UI fixtures.
  The Cardputer ADV build, host checks, and stack checks passed.
- Flashed the deck app partition with a verified hash. The deck booted
  with SD ready and the Grove link ready. On the USB-injected keyboard
  path, Help blocked navigation and both h and Back closed it. From a
  live passive Wi-Fi snapshot, f opened Hunt. First Back closed the
  guide while Hunt stayed active, second Back exited Hunt. Returned to Link.
  Physical visual and keyboard checks remain open.

## 2026-11-23 — USB use/idle rerun passed

- The unchanged flashed pair finished all six use/idle cycles in 3,605.37 s.
  Across 253 samples, zero reported link errors, timeouts, resets, UART
  errors, SD write errors, log drops, panics or telemetry retries. Minimum
  loop stack headroom was 2,560 B. The six closed LoRa recordings contained
  310 packet rows in total. Cleanup confirmed both lanes idle and logs closed.
- This closes the three USB phases for the tested image across the steady
  and switching run and the corrected use/idle rerun. Battery testing,
  measured power, and the later physical keyboard timeout check remain open.

## 2026-11-22 — Display timeout keyboard handling

- Moved timeout evaluation after input so a key at the deadline is handled
  while the screen is still awake. Raw keyboard state keeps the display
  awake while a key is held and wakes on modifier-only input. Suppression
  lasts until all keys in the wake chord are released.
- Host regressions cover the deadline, held keys and wake chord release.
  A copy without wake-chord suppression failed the regression. The
  protocol suite, Cardputer ADV build and stack checks passed. Left the currently flashed
  image running its USB use/idle phase; physical-key verification is pending.

## 2026-11-22 — Current-firmware USB endurance and use/idle retry

- Completed two-hour steady (7,200.37 s) and 30-minute passive scan switching
  (1,800.43 s). Closed the continuous LoRa recording with 1,600 packet and
  903 health rows. Reported zero link errors, timeouts, probe resets, UART
  errors, SD errors, and log drops. Minimum sampled loop stack was 2,592 B.
- The first use/idle cycle failed its Wi-Fi ownership check during scripted
  navigation. LoRa was still recording. Cleanup closed it and left both
  lanes idle. No panic signature appeared in the timed run.
- Bench replay showed the sequence can lose its dossier entry when Enter is
  consumed by display wake and the next Back is still sent. In a forced-sleep
  replay the deck also briefly lost its cached state; that replay did not
  capture a reset diagnostic. The collector now explicitly wakes the display,
  verifies dossier/System transitions, and reports the exact mismatched fields.
  Started a use/idle-only rerun on the same firmware. Two setup checks
  exposed native USB bootloader entry and a missing deck LoRa configuration
  after boot. Recovered the deck with a watchdog reset, opened the port
  without changing modem lines, and configured LoRa before the timed
  baseline. The first corrected UI pass preserved Wi-Fi and LoRa with
  zero reported errors or drops; the full hour remains in progress.

## 2026-11-22 — Roadmap reconciliation and 802.15.4 recovery

- Rechecked D-18 on the attached deck and probe with LoRa idle. The passive
  Wi-Fi scan before 802.15.4 found 106 APs. Stopping 802.15.4 restarted the
  probe (uptime 63,601 → 848 ms), and Grove was ready within 4.1 s. A fresh
  passive scan returned 113 APs with zero reported link errors or timeouts.
- The first attempt waited on the deck's reset counter, which counts only
  unsolicited HELLO while Ready. Used probe uptime on the repeat to capture
  a reboot that recovered through a solicited handshake.
- Updated the roadmap with bench evidence and remaining gates for wardrive,
  probe persistence, LoRa sweep, baseline watch, Signal Hunt, and audio.

## 2026-11-22 — Audio Cues settings copy

- The existing Audio Cues row now shows an Enter hint on both displays.
  The external inspector lists startup, Signal Hunt, and waypoint sounds.
  Rendered 171 fixtures and checked the audio inspector for clipping.
- Built the deck and passed stack guards. Flashed with a verified hash.
  Enter from the Audio Cues row toggled mute off and back on through the
  normal keyboard path over USB. The external TFT redrew after the change.

## 2026-11-22 — Synthesized sci-fi startup sound

- Replaced the four-note chime with 1.8 s of original mono PCM. A D minor
  pad swells under the rising bell motif and fades to silence. The linker
  places all 28,800 samples in flash. The exported PCM peaked at 76/127.
- Cardputer ADV build and stack guards passed. Flashed the deck app partition
  with a verified hash and booted with audio enabled. The probe link returned
  ready with zero errors, timeouts, or resets. Restored the previous mute
  setting after the audition. Acoustic playback awaits listener confirmation.

## 2026-11-22 — Deck boot melody and event sounds

- Added a four-note boot melody, rising Hunt acquisition and falling stale-target
  cues, and a three-note saved-waypoint confirmation. All use the saved audio
  setting. Cues run from the main loop, with Geiger pulses waiting until the
  active cue finishes. Muting clears the cue and stops its speaker channel.
- Built the Cardputer ADV image, passed stack guards (loop 1072/2048 B), and
  flashed the deck app partition with a verified hash. On the updated image,
  live Wi-Fi Hunt acquired a target and a GNSS waypoint saved to SD. Closed
  the test session and restored mute. The Grove link recorded zero errors,
  timeouts, or probe resets. New sounds still need a listening check.
- Host cue tests cover transition direction, repeated-state suppression,
  timing across clock wrap, boot note count, and cancellation. A copy with
  the Hunt transition check removed failed the test as expected.

## 2026-11-22 — Signal Hunt audio bench check

- Ran three enable/mute cycles with the attached deck and probe and a live
  Wi-Fi target. The deck reported the requested audio state at each step,
  Hunt stayed active, and samples advanced through both states (372 total).
  Stopped Hunt, restored the previous audio preference, and returned to Link.
- At the observed −56 to −57 dBm, the cadence model calls for roughly two
  pulses per second. Host cadence and stale-signal mute tests passed.
- Repeated all three cycles with the operator present, who confirmed the
  sound check. Hunt received 342 samples during the repeat and the probe
  returned to idle on stop.

## 2026-11-22 — Passive 802.15.4 bench smoke test

- Started passive 802.15.4 over the deck console. The deck showed
  `zig_active=1` and probe owner `ieee802154`. After approximately 10 s,
  it reported zero PANs and zero nodes. Stop released the lane and returned
  the probe to idle with zero link errors, timeouts or resets.
- The first script expected the wrong owner label and timed out while
  observing the active receiver. The protocol defines `ieee802154`.
  Live frame decode and PAN baseline behavior still need known traffic.

## 2026-11-22 — Wardrive resume bench recheck

- Checked clean main at `572a512`; both USB devices matched their expected IDs.
  `check_protocol.sh` and all 27 OCP REPL self-tests passed.
- Exercised the already-flashed deck and probe over Grove. The deck exposed
  the new `wardrive_block` field. A passive Wi-Fi Signal Hunt paused the
  wardrive stream while its SD log stayed open. The stream resumed within
  4.1 s of stopping Hunt. The test log closed and the probe returned to idle.
- With GNSS fixed, the session counted 240 AP rows, zero no-fix AP rows, zero
  SD write errors, and zero link errors, timeouts or probe resets during the
  test. Duplicate suppression passed its host regression; the SD records were
  not inspected for duplicates. No firmware was reflashed or flash hash checked.

## 2026-11-22 — Settings bench smoke test

- Entered Settings from Link via the normal Back action over USB and moved
  through all six rows without changing settings. The external TFT pixel
  counter advanced on entry and on all five cursor moves. Returned to Link.
  Physical layout on either panel was not visually inspected.
- The attached deck reported two built-in LoRa profiles and zero SD profiles
  loaded. SD profile loading and on-device vendor lookup remain open.

## 2026-11-22 — First flash of WP-3..12b + review fixes: bench results & two hardware-found fixes

- **Flashed:** probe `38:44:BE:BF:D2:94` (`build-uart` at `414e273`, 3/3 hashes) and deck
  `50:78:7D:CE:6D:64` (`cardputer-adv`, 4/4 hashes; reflashed with the fixes below).
  Driven over the deck debug console through one held-open USB connection.
- **Verified on hardware:**
  - Link ready, 0 errors/timeouts/resets across the session; probe heap steady (min 71 KB).
  - Baseline false-downgrade fix: 130-AP scan baseline reloaded from SD after reboot,
    event-only live rows → **0 `AuthDowngrade`**, 0 `MfpDropped`; 3–4 `EvilTwin` (known
    mesh/dual-band behaviour, open item).
  - Wi-Fi Signal Hunt: ~9 Hz samples (10 Hz cap), owner `wifi`. BLE hunt: ~6.5 Hz, owner `ble`.
  - LoRa sweep at priority 2 with `CalibrateImage`: US 902–928, EU 863–870 and
    `CELL 869-894` all ran (~260 ms per 64-bin sweep; peaks −60/−76/−72 dBm), `hw_fault=0`.
  - Probe NVS: after `reboot`, `config: loaded region=EU band=auto custom_ch=0 lora=1` and
    Wi-Fi came up in EU (`cc=DE`, 13 ch) before any deck push. Region restored to US.
  - Wardrive pause/resume: hunt interrupts → `PAUSED` (blocker `engine`) → stream resumes
    ~3 s after the hunt closes.
- **Fixed — resume never fired (`app/wardrive_resume.h`, `app/deck_wardrive.cpp`):** the
  deck's ~2 s status poll briefly marks the client busy, and any busy moment restarted the
  3 s settle, so it never elapsed. PHY use (tool, hunt, owner, handoff) still restarts the
  wait; a busy client, stop, paging, retry or row drain now only defers the send.
  `wardrive_resume_test` covers it (old behaviour as a mutant fails). `dump` gains
  `wardrive_block` (which condition holds resume) and `baseline_down/twin/mfp`.
- **Fixed — legacy page logger ran alongside `start_wardrive` (WP-3):** stream-added scan
  rows were re-logged with the last scan page's fix: counted `nofix=269` with a live GNSS
  fix in one run, and would write stale-position duplicates once a page fix is valid.
  With the stream supported, it owns AP rows; page logging is the fallback only. After:
  `aps=267 nofix=0 bt=145 bt_nofix=0`. `test_wardrive_scan_cursor.py` gains a stream case
  and mutation.
- **Noted:** `r` (manual scan) during a drive is refused with "stop live scan first", since
  the stream runs the Wi-Fi model in live mode.
- **Not yet verified:** Geiger audio cues (needs an operator listening), WiGLE import of the
  BLE rows, sweep accuracy vs. a reference, and the deferred review items.

## 2026-11-22 — Band-aware SX1262 image calibration, cell-band sweep preset & wardrive auto-resume

- **SX1262 `CalibrateImage` (`lora_radio.c`, `lora_sweep_plan.{h,c}`,
  `lora_recon.c`, `tools/check_rx_only.py`):** power-on calibration only covers
  902–928 MHz, so EU (863–870) RX and sweeps ran uncalibrated. Bring-up now
  issues `CalibrateImage` (`0x98`, added to both RX-only allowlists with operator
  approval; a calibration command, not transmit-capable) from STDBY_RC for the
  sweep span or the RX frequency, 4 MHz units with a 4 MHz guard each side
  (`lora_image_cal_bytes()`, host-tested; dropping the low guard fails
  `lora_sweep_plan_test`). OCP-SPEC §13 notes it.
- **Sweep presets (`model/lora_sweep_model.h`):** replaced `ISM 433` (outside the
  Wio's 862–930 MHz spec, Rev D §4) with `CELL 869-894`, the US 850 cellular
  downlink (energy only, no demodulation). Sweep bounds stay at the chip's 150–960.
- **Wardrive auto-resume (`app/wardrive_resume.h`, `app/deck_wardrive.cpp`,
  `app/deck_app.{h,cpp}`, `ui/gnss_view.{h,cpp}`):** an open Drive log whose
  `start_wardrive` stream was interrupted by another PHY tool now shows `PAUSED`
  and restarts the stream once the link, client and PHY lane (and probe owner)
  have been idle for 3 s. Manual `scan_networks` no longer flips the log onto the
  legacy Wi-Fi-only loop; that loop runs only after a probe answers `unknown`.
  Auto-start refusals log instead of raising a notice; a start timeout is retried
  by the resume timer instead of falling back. `wardrive_paused` added to `dump`.
  New `wardrive_resume_test` (removing the busy-restart fails it).
- **Checks:** `check_protocol.sh` green; `build_firmware.sh` both boards. The two
  warnings in this build are in M5GFX/M5Unified, which recompiled in full this time.
- **Not yet verified on hardware:** image-cal effect on EU/cell-band noise floor,
  and the pause/resume handoff with Signal Hunt and Wi-Fi Scan.

## 2026-11-22 — Pre-flash review of WP-3..12b: baseline downgrade fix & sweep priority

- **Review scope:** the seven unflashed commits `0bf4345..7beff73` (WP-9a, 12a,
  11, 5-probe, 3, 8, 12b). Receive-only posture re-confirmed: the only new SX1262
  opcode is read-only `GetRssiInst`; BLE hunt/wardrive go through the passive
  `start_disc()` choke point; Wi-Fi hunt/wardrive are promiscuous RX only.
- **Fixed — false `AuthDowngrade` (`model/baseline_watch.{h,cpp}`,
  `model/scan_model.{h,cpp}`):** beacon events overwrite a row's auth with the
  coarse `OPEN`/`WEP`/`RSN` label, which ranked below a scan baseline's `WPA3`,
  `WPA2/WPA3` and `*-EAP`, so every such AP flagged as downgraded once continuous
  scan or the wardrive stream ran (WPA1 APs fell to `WEP` likewise). Rows now carry
  `auth_coarse`; the check compares the baseline against the highest rank the coarse
  label could hide (`OPEN`=0, `WEP`=2, `RSN`=4). The existing test missed it because
  its baseline was itself captured from events; new case uses a precise scan
  baseline vs event rows. Mutation proof: restoring `authRank(r.auth)` fails
  `baseline_watch_test`.
- **Fixed — LoRa sweep outranked the event sender (`lora_recon.c`,
  `ocp_frame.{h,c}`):** `lora_sweep` ran at priority 4 with ~1.1 ms busy-wait per
  bin, above `ocp_evt` (3), so other lanes' events could drop while sweeping. Sweep
  now runs at `LORA_SWEEP_TASK_PRIO` 2, enforced by a `_Static_assert` against the
  new `OCP_EVENT_TASK_PRIO`. Mutation proof: priority 4 fails the probe build on
  that assert.
- **Checks:** `check_protocol.sh` green, `build_firmware.sh` both boards, 0 warnings.
- **Open, not yet changed:** no `CalibrateImage` for 863–870 MHz (power-on calibration
  is 902–928 only) and the `ISM 433` sweep preset is outside the Wio's 862–930 MHz
  spec (Rev D §4); the `start_wardrive` stream is not resumed after another PHY tool
  interrupts it while the log stays open.

## 2026-11-22 — Single-target Signal Hunt & muteable Geiger audio cadence (WP-12b)

- **Protocol & Probe (`protocol/ocp.h`, `protocol/OCP-SPEC.md` §14,
  `hunt_filter.{h,c}`, `wifi_hunt.{h,c}`, `ble_recon.{h,c}`, `ocp_frame.{h,c}`,
  `ocp_server.c`):**
  - Added strictly receive-only `hunt_wifi <mac> <ch>` (`OCP_CC_WIFI`, 2 args)
    and `hunt_ble <mac>` (`OCP_CC_BLE`, 1 arg) plus `[EVT] kind=hunt mac=...
    rssi=... ch=...` (`OCP_EVENT_RECORD_HUNT`).
  - Added pure-C `hunt_filter.{h,c}` (strict `aa:bb:cc:dd:ee:ff` MAC parser,
    802.11 management/data `Addr2` matcher rejecting control/short frames, and
    wrap-safe 100 ms rate limiter) with ASan/UBSan host tests (`hunt_filter_test.c`).
  - Added `wifi_hunt.{h,c}` locking `PHY_OWNER_WIFI` onto one region-validated
    channel in passive promiscuous RX (`MGMT | DATA`, zero probe requests) and
    extended `ble_recon.c` with `BLE_MODE_HUNT` routed through the single
    audited `start_disc()` choke point (`params.passive = 1`, `filter_duplicates = 0`).
- **Deck (`model/signal_hunt.{h,cpp}`, `storage/settings.{h,cpp}`,
  `ui/signal_hunt_view.{h,cpp}`, `ui/external_hunt_view.{h,cpp}`,
  `ui/settings_state.h`, `ui/settings_view.cpp`, `ui/external_card_views.cpp`,
  `ui/external_gps_display.{h,cpp}`, `app/deck_app.{h,cpp}`, `main.cpp`):**
  - Added allocation-free `model::SignalHunt` (64-sample RSSI sparkline ring,
    peak/min/stale tracking, and monotonic Geiger pulse scheduler mapping
    `-100..-35 dBm` to `1200..85 ms` and `520..2200 Hz`, silenced when muted
    or stale `> 5 s`).
  - Added persisted `/oscilla/audio_enabled` (default **muted / off**) and a
    7th Settings row (`AUDIO CUES`) in both the onboard scrollable Settings
    menu and the Observatory v2 external inspector.
  - Wired `f` hotkey on Wi-Fi Scan, AP Dossier, BLE Scan, BT Dossier, and
    Anti-Surveillance to enter Signal Hunt; `a` live mute toggle; `r` peak
    reset; `` ` `` exits and stops the PHY lane; `hunt wifi|bt|stop` and
    `audio on|off` debug commands; `M5Cardputer.Speaker.tone()` output in
    `main.cpp`.
  - Built onboard 240×135 (`signal_hunt_view.cpp`) and Observatory v2 320×240
    (`external_hunt_view.cpp`) Signal Hunt views with sparkline, peak dashed
    line, 24-segment proximity meter, Geiger indicator, and Reading Dock.
- **Future roadmap notes captured (`ROADMAP.md`):** recorded post-v1 ideas for
  expanded muteable speaker cues and D-8-respecting vendor DB / SD log sync
  (USB-C zero-RF host tool vs. isolated Dock/Sync OTA partition).
- **Checks:** `check_protocol.sh` (ASan/UBSan `hunt_filter_test` +
  `signal_hunt_test`, `check_rx_only.py` across 276 files + 6 rejected TX
  mutations, `check_external_frame_memory.py`, `ocp_repl.py --selftest` 27/27,
  `check_ui_palette.py`), two mutation proofs (disabling 802.11 control-frame
  rejection in `hunt_filter.c` and disabling stale mute in `signal_hunt.cpp`
  both caught), `render_external_views.py` (171 fixtures; visually inspected
  `signal-hunt-active.png`, `signal-hunt-empty.png`, `deck-signal-hunt-active.png`,
  and `settings-audio-active.png`), and `build_firmware.sh` (`oscilla-c5.bin`
  20% app partition free; `cardputer-adv` RAM 22.8% / Flash 28.8%, `loop()`
  stack 1072/2048 B).
- **Not yet verified on hardware:** live `hunt_wifi` / `hunt_ble` RF RSSI
  cadence, speaker loudness/tone feel, and combined post-WP-3..12b heap check
  on physical boards (pending next flash).

## 2026-11-21 — SX1262 passive sub-GHz `GetRssiInst` spectrum & waterfall sweep (WP-8)

- **Protocol & Probe (`protocol/ocp.h`, `protocol/OCP-SPEC.md` §13,
  `lora_sweep_plan.{h,c}`, `lora_radio.{h,c}`, `lora_recon.{h,c}`,
  `ocp_frame.{h,c}`, `ocp_server.c`):**
  - Added strictly receive-only `lora_sweep [start_khz] [step_khz] [bins]`
    (`OCP_CC_LORA_RX`, `OP_GET_RSSI_INST = 0x15` allowlisted as read-only in
    both `lora_radio.c` and `tools/check_rx_only.py`, keeping all forbidden TX
    opcodes blocked).
  - Extracted shared `bringup_standby_xosc_locked()` in `lora_radio.c`;
    `lora_radio_sweep_start()` configures wide 500 kHz IF bandwidth with DIO1
    IRQs masked and `RF_SW=RX`, mutually exclusive with packet RX;
    `lora_radio_sweep_measure()` steps each frequency bin, settles 600 µs, and
    samples `GetRssiInst` 3× taking the max dBm (`-raw/2`), yielding every 8
    bins so the single-core C5 never starves USB/UART.
  - Emits `[EVT] kind=lora_sweep start_khz=... step_khz=... bins=... rssi=...`
    via non-blocking `ocp_event_submit()` (<= 382 bytes for 64 bins, well under
    `OCP_MAX_LINE_LEN = 512`), reports `lora=sweep` in `[STATUS]`, and tears
    down cleanly on `stop lora`.
- **Deck (`model/lora_sweep_model.{h,cpp}`, `ui/subghz_view.{h,cpp}`,
  `ui/external_lora_view.{h,cpp}`, `ui/external_gps_display.{h,cpp}`,
  `app/deck_app.{h,cpp}`, `app/link_reconcile.h`):**
  - Added allocation-free `LoraSweepModel` (64-bin current + peak-hold +
    25th-percentile noise floor + 24-row waterfall history ring) with US
    902–928 / EU 863–870 / ISM 433 presets.
  - Added `w` toggle on Sub-GHz screen (`s` start/stop sweep, `x` band preset,
    `r` clear peak hold, `lora sweep` debug command) and Observatory v2 320×240
    **Sub-GHz Spectrum & Waterfall** companion view (`drawExternalLoraSweepStrip`).
- **Checks:** `check_protocol.sh` (ASan/UBSan `lora_sweep_plan_test` +
  `lora_sweep_model_test`, `check_rx_only.py` across 266 files + 6 rejected TX
  mutations, `check_ui_palette.py`, `check_external_frame_memory.py`), two
  mutation proofs (disabling 960 MHz upper-span check and disabling peak-hold
  retention both caught), `render_external_views.py` (visual inspection of
  `lora-sweep-active.png`), and `build_firmware.sh` (both boards + stack guards).
- **Not yet verified on hardware:** live SX1262 `GetRssiInst` sweep cadence
  and noise-floor readings on physical Wio-SX1262.

## 2026-11-21 — Probe-driven multi-band `start_wardrive` & WigleWifi-1.6 BLE CSV (WP-3)

- **Probe (`firmware-c5/main/wardrive_cache.{h,c}`, `wifi_wardrive.{h,c}`,
  `ble_recon.{h,c}`, `radio_arbiter.{h,c}`, `ocp_server.c`, `protocol/ocp.h`):**
  - Added `PHY_OWNER_WARDRIVE` / `OCP_OWNER_WARDRIVE` (`"wardrive"`), modelling
    Wi-Fi + duty-cycled BLE coexistence as a single combined PHY owner (DESIGN §6.2).
  - Added pure-C `wardrive_cache.{h,c}` (256 Wi-Fi + 128 BLE slots, TTL
    re-emission, `>= +10 dB` RSSI-jump re-emission, clock-wrap safe, LRU
    eviction when full) so moving drives never stall on fixed tables.
  - Added `wifi_wardrive.{h,c}` handling `start_wardrive [ble] [wifi_dwell_ms]
    [ble_dwell_ms] [ttl_s]` (OCP-SPEC §10.10): sequentially alternates passive
    Wi-Fi hopping (`beacon_parse`, zero probe requests) with bounded passive
    BLE sweeps via `ble_recon_wardrive_window()` routed through `ble_recon.c`'s
    single audited `start_disc()` choke point (`params.passive = 1`), emitting
    `[EVT] kind=network` and `[EVT] kind=ble` through non-blocking
    `ocp_event_submit()`, with <= 25 ms stop responsiveness.
- **Deck (`storage/wardrive_csv.{h,cpp}`, `storage/wardrive_logger.{h,cpp}`,
  `model/session_review.cpp`, `app/deck_app.{h,cpp}`, `app/deck_wardrive.cpp`,
  `app/deck_debug_snapshot.cpp`, `ui/gnss_view.cpp`):**
  - Added 14-column WigleWifi-1.6 `wardriveCsvBtRow()` (`Type=BLE`, `Channel=0`,
    `Frequency=0`, `MfgrId` populated, hostile name/formula neutralization) and
    `[RSN][ESS]` mapping for continuous-discovery Wi-Fi rows; updated
    `session_review.cpp` so LOGS reads mixed `WIFI` + `BLE` Drive CSVs cleanly.
  - Wired `DeckApp::toggleWardriveLog()` to start `OCP_V_START_WARDRIVE` with
    bounded receipt-time-geotagged Wi-Fi/BT queues drained 1 row/loop (falling
    back automatically to legacy `scan_networks` on `code=unknown` from older
    probes), preserving `test_wardrive_scan_cursor.py` unchanged.
- **Checks:** `check_protocol.sh` (ASan/UBSan `wardrive_cache_test`, expanded
  `wardrive_csv_test` 24/24, `session_review_test`, `check_rx_only.py` across
  262 source files, `test_wardrive_scan_cursor.py`), two mutation proofs
  (disabling LRU eviction and altering `Type=BLE` column both caught),
  `render_external_views.py`, and `build_firmware.sh` (both boards + stack guards).
- **Not yet verified on hardware:** live `start_wardrive` Wi-Fi+BLE cycling,
  stop teardown latency, and real WiGLE CSV import check on reflashed boards.

## 2026-11-21 — Probe NVS configuration persistence (WP-5)

- Added pure-C `firmware-c5/main/config_codec.{h,c}` implementing a fixed
  128-byte versioned, little-endian, IEEE-CRC32-protected configuration blob
  (`"OCFG"` v1) with strict bounds validation for Wi-Fi region, band scope,
  region/band-constrained custom channel lists (DFS-free), and SX1262 LoRa RX
  parameters. Any corrupt/truncated/invalid blob falls back cleanly to safe
  compile-time defaults (`US`, `auto`, all channels, LoRa unset so D-9 still
  holds on a fresh probe).
- Added ESP-IDF NVS wrapper `firmware-c5/main/config.{h,c}` (`oscilla`/`cfg_v1`),
  loaded in `app_main()` before `wifi_recon_init()` (so Wi-Fi boots directly
  into the saved country/band/channels), restoring saved LoRa RX params via
  `lora_recon_apply_saved()` after `lora_recon_init()`, and committing on
  successful `set_region`, `set_band`, `set_channels`, and `lora_config`.
- **Checks:** `check_protocol.sh` (ASan/UBSan `config_codec_test` +
  `check_rx_only.py`), two mutation proofs (disabling CRC32 check and disabling
  region channel check both fail `config_codec_test`), and `build_firmware.sh`
  (both `esp32c5` + `cardputer-adv` clean).
- **Not yet verified on hardware:** live probe NVS persistence across power-cycle/reboot.

## 2026-11-21 — Passive baseline & evil-twin / anomaly watch (WP-11)

- Added pure-C++ `model/baseline_watch.{h,cpp}` (`BaselineWatch`, capped at 256
  APs / 32 802.15.4 PANs / 64 anomalies) comparing live passive Wi-Fi `[SCAN]`,
  `[EVT] kind=network`, `[INSPECT]`, and `[ZIG]` observations against an
  operator-captured baseline. Flags conservative deltas: `NewAp`, `NewPan`,
  `MfpDropped`, `AuthDowngrade` (recognizing `OPEN` < `WEP` < `WPA` < `WPA2`/`RSN`/`OWE`
  < `WPA3`), and `EvilTwin` (known non-empty baseline SSID from an unfamiliar
  BSSID; empty/hidden SSIDs never false-positive), with high-severity eviction
  protection when the anomaly list hits 64.
- Added `storage/baseline_store.{h,cpp}` persisting `/oscilla/baselines/default.csv`
  (32 KiB cap, hostile-SSID hex escaping, strict `schema=oscilla-baseline-v1`
  header validation) under `storage::lock()`, loaded once at boot.
- Wired into `DeckApp` (`b` key on Wi-Fi Scan toggles save/clear,
  `baseline save|load|clear` debug commands, `baseline_*` fields in `dump`),
  onboard `sweep_view.cpp` (`BL OK` / `BL +N` / `BL !N` badge + row tinting),
  and external `external_wifi_view.cpp` via a zero-heap POD `WifiBaselineBanner`
  in `ExternalCardState`.
- **Checks:** `check_protocol.sh` (ASan/UBSan `baseline_watch_test` +
  `test_sweep_layout.py` + `check_external_frame_memory.py` + `check_rx_only.py`),
  two mutation proofs (disabling evil-twin check and disabling auth-downgrade
  check both fail `baseline_watch_test`), `render_external_views.py`, and
  `build_firmware.sh` (both boards + stack guards).
- **Not yet verified on hardware:** on-device `b` toggle, SD baseline reload,
  and live anomaly display review.

## 2026-11-21 — Offline SD OUI & Bluetooth Company-ID vendor lookup tables (WP-12a)

- Added pure-C++ `model/vendor_table.h` (sorted 24-byte binary record reader,
  16-byte header/truncation validator, MAC OUI + LAA parser, Bluetooth SIG
  16-bit Company-ID parser, ASCII sanitizer, bounded <=24-step binary search,
  and fixed 32-slot RAM LRU cache caching both hits and misses).
- Added SD service `storage/vendor_lookup.{h,cpp}` querying `/oscilla/oui.bin`
  and `/oscilla/bt_mfr.bin` under `storage::lock(2)` with absent-table
  memoization so cards without vendor files pay zero SPI overhead.
- Added stdlib-only `tools/build_vendor_tables.py` converting IEEE OUI CSVs
  and Bluetooth SIG company identifier YAML/CSVs into Oscilla's binary tables
  (`--selftest` cross-checks generated binaries against the compiled C++ lookup).
- Wired once on dossier open (`DeckApp::openWifiDossier()` / `openBtDossier()`)
  into `model::WifiInspection` / `model::BtInspection` and rendered in both
  onboard and external `wifi_detail_view.cpp` & `bt_detail_view.cpp` (zero SPI
  I/O during strip redraws, clean fallback when absent).
- **Checks:** `check_protocol.sh` (new `vendor_table_test` +
  `build_vendor_tables.py --selftest --cpp-bin` + `check_rx_only.py`), mutation
  proof (relaxing truncation guard fails `vendor_table_test`), and
  `build_firmware.sh` (both boards + stack guards).
- **Not yet verified on hardware:** live SD card lookup with real IEEE/BT-SIG tables.

## 2026-11-21 — Settings menu overhaul (paired 320×240 inspector) & SD-loaded LoRa profiles (WP-9a)

- **Settings menu overhaul (`ui/settings_state.h`, `ui/settings_view.{h,cpp}`,
  `ui/external_card_views.{h,cpp}`, `ui/external_gps_display.{h,cpp}`):**
  - Replaced the cramped two-panel onboard Settings layout with a clean 6-row
    scrollable list (`;`/`.` moves cursor, `,`/`/` or `Enter` adjusts/cycles,
    `` ` `` returns to Link): `BRIGHTNESS`, `IDLE SLEEP`, `WI-FI REGION`,
    `WI-FI BAND`, `LORA PROFILE`, and `DEBUG SERIAL`.
  - Replaced the brightness-only 320×240 external Settings screen with a full
    Observatory v2 paired configuration inspector that dynamically renders a
    dedicated visual control for the selected row (16-segment luminance bar,
    5-pill idle timer selector, regulatory domain + channel list, 3-way band
    selector, LoRa frequency/SF/BW/CR/sync dossier, or USB debug console status)
    over a 3-bay device-wide summary dock.
- **SD-loaded LoRa profiles & persistence (WP-9a, `model/lora_profile_catalog.{h,cpp}`,
  `storage/settings.{h,cpp}`):**
  - Added pure-C++ `LoraProfileCatalog` reading `/oscilla/lora_profiles.csv`
    (`token,label,freq_hz,sf,bw_khz,cr,sync_word`), strictly validated against
    SX1262 limits (`150–960 MHz`, `SF5–12`, valid SX1262 BWs, `CR1–4`,
    `0x00–0xFF`), rejecting hostile/duplicate rows and capping at 16 profiles
    while keeping built-in `MCORE`/`MTASTIC` authoritative.
  - Persisted selected LoRa profile token in `/oscilla/lora_profile` and
    converted `PendingLoraConfig::profile_token` to owned `std::string`.
- **Checks:** `check_protocol.sh` (new `lora_profile_catalog_test` + palette
  & frame-memory checks), mutation proof (disabling SX1262 BW validation fails
  `lora_profile_catalog_test`), `render_external_views.py` (163 fixtures
  including onboard & all 6 external Settings pages, visually inspected for
  clean margins and zero clipping), and `build_firmware.sh` (both boards +
  stack guards: `loop()` 1040/2048 B).
- **Not yet verified on hardware:** on-device Settings visual & SD CSV load
  verification (queued for next bench flash).

## 2026-11-21 — Flash & live bench verification of WP-5 & WP-10 (+ lazy LoRa KML fix)

- Flashed updated deck firmware (`cardputer-adv`, `50:78:7D:CE:6D:64`, 4/4 hashes
  verified; probe `38:44:BE:BF:D2:94` unchanged from `7687ef1`).
- **Hardware-discovered latency fix (`firmware-cardputer/src/storage/lora_logger.cpp`):**
  - On the first bench run against the 64-session SD card, eagerly opening
    `log_NNNN.kml` + 4 redundant `SD.exists()` probes inside `loraLogBegin()`
    pushed `max_cycle_us` to **2,122,736 µs (2.12 s)**, tripping one 2 s OCP
    timeout (`link_timeouts=1`).
  - Fix: open `log_NNNN.kml` **lazily** on the first geotagged packet or
    waypoint (`ensureLoraKmlOpen()`), keep one `SD.exists(g_packet_path)` guard
    (since `nextFreeSession()` already scanned all extensions in one `readdir`
    pass), and skip redundant pre-`close()` flush on `.meta`.
  - Reflashed and retested: `max_cycle_us` dropped to **1.67 s** with
    **`link_timeouts=0`**, **`link_errors=0`**, **`write_errors=0`**.
- **Live bench results (both boards connected, live GNSS fix `age_ms=46..283`, `chkfail=0`):**
  - **WP-5 idle display sleep:** `idle 30` entered `display_sleeping=1` and
    `tft_status initialized=1 sleeping=1` at 30 s while `wifiscan` kept
    receiving (`wifi_live=1`, `scan_rows=161`, `gnss=fix`, `link_timeouts=0`);
    pressing `` ` `` logged `display wake (key swallowed)`, restored
    `display_sleeping=0` / `tft_status sleeping=0`, and left `wifi_live=1`
    untouched. Restored `idle 0`.
  - **WP-10 geotagged LoRa + KML + waypoint:** live MeshCore packet received
    with GNSS fix → `packet_rows=1`, `lora_geotagged=1`, `mark` → `waypoint
    drive=0 lora=1`, `verify_lora_log` → `state=match packet_bytes=344
    health_bytes=130`, `health_rows=2`, `write_errors=0`.
  - **WP-10 LOGS sidecar inspection:** opened the newly recorded LoRa session
    in LOGS → `session_kind=1`, `session_parts=15` (`CSV|KML|META|HEALTH`),
    `session_meta=1`, `session_health=1`, `session_geotagged=1`,
    `session_valid=1`, `session_bad=0`, `session_error=0`, `link_timeouts=0`.
  - **Memory & final state:** deck `heap_free=174,872 B`, `heap_min=163,268 B`,
    `stack_free_min=2,608 B`; probe `heap=98,148 B`; returned to Link card with
    `debug off`, `idle_timeout_s=0`.
- **Checks after fix:** `check_protocol.sh` (all green), `build_firmware.sh`
  (both boards + stack guards), reflashed and bench-verified.

## 2026-11-21 — Geotagged LoRa KML, session waypoints & LOGS sidecar inspection (WP-10)

- **Geotagged LoRa CSV + incremental SNR-colored KML (`storage/lora_log_format.{h,cpp}`,
  `storage/lora_logger.{h,cpp}`):**
  - Extended LoRa session CSV to 16 columns (`...,lat,lon,alt,hdop,fix_age_ms,utc`);
    rows without a fresh valid GNSS fix leave the six GNSS columns empty and never
    fabricate coordinates or dates.
  - Added incremental KML 2.2 coverage logs (`log_NNNN.kml`) alongside `.csv`,
    `.meta`, and `.health.csv` (SNR buckets: green `> 0 dB`, yellow `-10..0 dB`,
    red `< -10 dB`, pink `sWaypoint`, zero remote `<href>`), preserving
    `nextFreeSession()`'s single-walk numbering guard.
- **Session waypoints (`storage/wardrive_kml.{h,cpp}`, `wardrive_logger.{h,cpp}`,
  `lora_logger.{h,cpp}`, `app/deck_app.{h,cpp}`):**
  - Added `m` hotkey on Drive and Sub-GHz views plus `mark` debug command to
    drop a fresh-fix `WP #N` KML placemark into any active Drive and/or LoRa
    session without interrupting reception.
- **LOGS sidecar inspection (`model/session_review.{h,cpp}`,
  `storage/session_reader.cpp`, `ui/session_view.cpp`):**
  - Updated `SessionParser` to accept both legacy 10-column and new 16-column
    geotagged LoRa CSVs (rejecting out-of-range coordinates) and recognize
    `log_NNNN.kml` as `SessionPart::Kml`.
  - Added pure-C++ `LoraMetaParser` and `LoraHealthParser` driven cooperatively
    in `session_reader.cpp` under `lock(0)` with the same `<= 512 B/tick`
    shared-bus budget; surfaced tuning provenance, cumulative `RX/CRC/DRP/FLT`,
    and geotag counts on the LOGS Summary view.
- **Checks:** `check_protocol.sh` (extended `lora_log_format_test` + Python
  `xml.etree` LoRa KML check, extended `wardrive_kml_test` + `check_wardrive_kml.py`,
  extended `session_review_test` and `session_reader_test`), `render_external_views.py`
  (154 fixtures + session highlight check), three mutation proofs (invalid fix
  coord leak, out-of-range latitude acceptance, disabled sidecar read), and
  zero-warning `build_firmware.sh` (both boards + stack guards: `loop()` 1040/2048 B).
- **Not yet verified on hardware:** live flash and bench test (next step).

## 2026-11-21 — Idle display sleep & measured battery discharge trend (WP-5)

- **Idle display sleep (`firmware-cardputer`):**
  - Added pure-C++ `app/idle_sleep.h` (`OFF`/`30s`/`60s`/`2m`/`5m`, default `OFF`,
    clock-wrap safe, swallows the waking key press so it never triggers an action
    on the active card).
  - Persisted under `/oscilla/idle_timeout` (`storage/settings.{h,cpp}`), wired
    to `t` on Settings (`ui/settings_view.{h,cpp}`), `idle <off|0|30|60|120|300>`
    debug command, and `dump`/`display_status`.
  - Added shared-lock-guarded `setExternalTftSleep(bool)` (`ui/external_tft.{h,cpp}`)
    and `setExternalGpsDisplaySleep(bool, now_ms)` (`ui/external_gps_display.{h,cpp}`):
    internal LCD turns backlight off + `sleep()`; external ST7789 enters `SLPIN`
    and halts SPI2 redraws (per Rev D §5.1, `BLK` stays on 3.3 V so GNSS is never
    interrupted) and invalidates the row diff cache on wake.
- **Measured battery burn-rate / runtime slope (`model/system_resources.h`):**
  - Added `model::BatteryTrend` (pure C++, zero heap): samples `M5.Power`
    periodically inside `DeckApp::tick()` (even while displays are asleep),
    requires >= 60 s elapsed and >= 2% discharging drop before reporting
    `%/h` and estimated remaining minutes (`EST`), resets anchor on charging or
    voltage bounce, and renders on onboard `ui/info_view.cpp` + `dump`.
- **Checks:** `check_protocol.sh` (new `idle_sleep_test`, extended
  `system_resources_test`, extended `check_external_tft.py` with sleep lock-bypass
  mutation), mutation proofs (wake-swallow disabled and `kMinDropPct=1` both fail
  host tests), and `build_firmware.sh` (both `esp32c5` and `esp32s3` + stack
  guards: `loop()` 1040/2048 B, `runDebugCommand()` 384/2048 B).
- **Not yet verified on hardware:** live display sleep/wake transition and
  battery discharge slope on the physical Cardputer ADV (planned next after WP-10).

## 2026-11-21 — WP-1 (`lora_listen` concurrent startup) hardware closure

- Tested `lora_listen` start/stop directly under active continuous `wifiscan`
  bursts on connected bench hardware (probe `38:44:BE:BF:D2:94` + deck
  `50:78:7D:CE:6D:64`): **6/6 cycles** succeeded cleanly in **~140 ms**
  round-trip with `link_timeouts=0` and zero probe errors.
- Root-caused the original 2026-11-20 timeout to the deck-side O(N²)
  `SD.exists()` session-numbering stall inside `loraLogBegin()` (~6 s with
  many saved sessions), which was already resolved by commit `858e6e7`
  (`nextFreeSession()` single `readdir` walk). No probe-side dispatch task
  churn is needed; marked closed in `ROADMAP.md`.

## 2026-11-21 — Flash & live bench verification of WP-2/WP-4/WP-6/WP-7 (+ two hardware-found fixes)

- Flashed both boards by USB serial ID with verified hashes:
  - Probe `38:44:BE:BF:D2:94` (`esp32c5`, `build-uart`, 3/3 hashes verified).
  - Deck `50:78:7D:CE:6D:64` (`cardputer-adv`, 4/4 hashes verified).
- **Two real bugs caught on the first bench pass and fixed immediately:**
  1. **ESP-IDF 5.5 `channel_bitmap` zero-band quirk (`firmware-c5/main/wifi_recon.c`):**
     `set_channels 1,6,11` under `band=auto` initially scanned in **7,784 ms**
     (109 APs) because ESP-IDF treats `ghz_5_channels == 0` under
     `WIFI_BAND_MODE_AUTO` as "scan all 5 GHz channels" rather than "skip
     5 GHz". Fix: temporarily narrow the Wi-Fi band mode to `2G_ONLY` (or
     `5G_ONLY`) for the duration of a single-band custom-bitmap scan and
     restore `wifi_band_scope()` on all exit paths (done, abort, start error).
     Retest on silicon: **`channels 1,6,11` dropped from 7,784 ms → 755 ms**
     (61 APs, matching 3 × 250 ms dwell).
  2. **Deck `channels` debug command busy drop (`firmware-cardputer/src/app/deck_app.cpp`):**
     `sendWifiChannels()` previously returned `"link busy"` when a background
     `status` poll happened to hold `client_.pending()`. Fix: queue via
     `retrySoon()`, matching all other deck commands.
- **Reflashed both boards and ran the full live bench suite (0 link timeouts,
  0 probe resets, 0 probe log/hop errors across 46 console lines):**
  - Link & handshake: `link=ready`, `probe_rst=poweron` (also confirms the
    2026-11-20 `rst=` addition on real probe silicon), `region=US/US`,
    `band=auto/auto`.
  - Band & channel filtering: `band 24` → 67 APs in 2,761 ms; `band 5` →
    66 APs in 7,028 ms; `channels 1,6,11` under `band=auto` → 61 APs in
    **755 ms**; `channels 52` → refused with `err code=badarg`; mid-`wifiscan`
    `band 24` deferred while busy (`probe_band=auto`) and applied automatically
    after `stop phy` (`probe_band=24`).
  - Enriched `inspect_network`: full dual-band scan (139 APs, 0 malformed,
    9,789 ms) + `inspect 1` captured 4 beacons cleanly.
  - Multi-vendor BLE passive scan & tracker watch: `scan_bt` saw 71 devices /
    13 trackers; 3 s `scan_airtag` recorded 8 sightings; clean stop.
  - D-UCB adaptive hopping (D-6 updated): 7 s `start_sniffer` (790 pkts,
    10 clients, 3 probes), 7 s `start_wifi_scan` (141 APs), 3 s
    `deauth_detector` (0 events), all with 0 hop errors.
  - Final memory: probe `heap=98,436 B`; deck `heap_free=118,308 B`,
    `heap_min=114,532 B`, `heap_largest=106,484 B`, `stack_free_min=3,312 B`.
- **Checks after fixes:** `check_protocol.sh` (all green), `build_firmware.sh`
  (both boards + stack guards), both boards reflashed and bench-verified.

## 2026-11-21 — D-UCB adaptive Wi-Fi channel picker (WP-4, D-6)

- Implemented `firmware-c5/main/wifi_channel_picker.{h,c}` (pure C, zero heap
  allocation, fixed 24-arm state) using discounted UCB1 (`gamma=0.95`,
  saturating per-dwell reward `hits / (hits + 2)`, exploration constant `0.90`,
  guaranteed initial full-list sweep so every active channel is sampled before
  adaptive weighting begins).
- Wired into `wifi_sniff.c` (frame hits), `wifi_networks.c` (beacon hits), and
  `wifi_deauth.c` (deauth/disassoc hits; degrades to balanced round-robin when
  quiet). `wifi_spectrum.c` (`channel_view`) stays uniform round-robin so
  per-channel density bars remain unbiased. Updated `DESIGN.md` §6.1.
- **Checks:** `check_protocol.sh` (new ASan/UBSan `wifi_channel_picker_test`
  covering single-channel safety, initial sweep, quiet balance, >=3x busy
  focus with starvation-freedom, and non-stationary shift adaptation),
  mutation proof (disabling discounting `GAMMA=1.0` fails shift adaptation),
  and `build_firmware.sh` (both `esp32c5` and `esp32s3` + stack guards).
- **Not yet verified on hardware:** live dwell distribution under real
  dual-band traffic.

## 2026-11-21 — Richer 802.11 beacon & RSN inspection (WP-7)

- Extended passive 802.11 beacon/probe-response parsing (`beacon_parse.{h,c}`)
  and `[INSPECT]` output (`wifi_inspect.c`, OCP-SPEC §10.3) with additive
  security and PHY posture keys (ignored by older decks):
  - **RSN AKM suites (`akm=`):** parses IEEE `00:0F:AC` AKM entries inside RSN
    IE 48 (`PSK`, `SAE`, `EAP`, `FT-PSK`, `FT-SAE`, `FT-EAP`, `OWE`).
  - **Channel width & generation (`width=`, `gen=`):** derives `20/40/80/160 MHz`
    and Wi-Fi 4/5/6 (`4`/`5`/`6`) from HT (45/61), VHT (191/192), and HE
    extension (255/35,36) IEs.
  - **Country IE & WPS (`cc=`, `wps=`):** validates 2-letter uppercase ASCII
    Country IE (IE 7) and Wi-Fi Alliance WPS vendor IE (`00:50:F2:04`,
    excluding WPA1 `00:50:F2:01`).
  - **Deck (`firmware-cardputer`):** parsed into `model::Inspect` in
    `scan_model.{h,cpp}` and rendered in the onboard `wifi_detail_view.cpp`
    dossier when a live passive capture populates them.
- **Checks:** `check_protocol.sh` (including 100k ASan/UBSan fuzzing on
  `beacon_test`, extended `model_test`, and `check_rx_only.py`), mutation
  proof (dropping the WPS type-4 check misclassifies WPA1 and trips
  `beacon_test`), and `build_firmware.sh` (both `esp32c5` and `esp32s3` +
  stack guards).
- **Not yet verified on hardware:** live `inspect_network` against physical
  Wi-Fi 6 / WPA3-SAE / WPS APs.

## 2026-11-21 — Multi-vendor BLE tracker classification (WP-6)

- Extended passive BLE tracker detection beyond Apple Find My (`airtag`) to
  also classify Google Find My Device Network (`fmdn`, 16-bit Service Data
  `0xFEAA` frame `0x40`/`0x41` — excluding plain Eddystone URL/TLM/UID),
  Samsung SmartTag (`smarttag`, `0xFD5A`), Tile (`tile`, `0xFEED`), and
  Chipolo (`chipolo`, `0xFE33`):
  - **Protocol:** added `OCP_TRACKER_*` literals in `protocol/ocp.h` and
    updated `protocol/OCP-SPEC.md` §11.4. `kind=airtag` stays unchanged with
    additive `tracker=<vendor>` so older decks stay compatible.
  - **Probe (`firmware-c5`):** extended `ble_adv_parse.{h,c}` AD walker for
    `0x16` Service Data and `0x02`/`0x03` 16-bit UUID lists, carried
    `ble_tracker_kind_t` through `ble_device_table.{h,c}`, `ocp_frame.{h,c}`,
    and `ble_recon.c`.
  - **Deck (`firmware-cardputer`):** updated `bt_model.{h,cpp}` (`tracker_kind`,
    `trackerShortLabel()` returning `AT`/`GFM`/`STG`/`TIL`/`CHP`),
    `anti_surveillance_model.{h,cpp}`, and `bt_view.cpp` (vendor badge in
    device rows and `LAST` tracker line).
- **Checks:** `check_protocol.sh` (including 100k ASan/UBSan fuzzing on
  `ble_adv_parse_test`, extended `bt_model_test`, and `check_rx_only.py`),
  mutation proof (dropping the FMDN frame-type filter trips `ble_adv_parse_test`),
  and `build_firmware.sh` (both `esp32c5` and `esp32s3` + stack guards).
- **Not yet verified on hardware:** live RF sightings against physical FMDN,
  SmartTag, Tile, and Chipolo hardware.

## 2026-11-21 — Wi-Fi band & custom channel filtering (`set_band` / `set_channels`, WP-2)

- Implemented the already-declared `set_band <24|5|auto>` and
  `set_channels <list|all>` verbs (OCP-SPEC §10.9) cleanly on top of the
  `set_region` channel table (`protocol/ocp_wifi_regions.h`):
  - **Protocol:** added `OCP_CHANNELS_ALL` to `protocol/ocp.h` and documented
    §10.9 plus additive `band=` on `[HELLO]`/`[STATUS]` in `protocol/OCP-SPEC.md`.
  - **Probe (`firmware-c5`):** extended pure-C `wifi_channels.{h,c}` with
    band scope (`auto`/`24`/`5`) and validated custom channel subsets
    (distinct channels strictly within active region and band; atomic rollback
    on invalid input; reset to `all` on region or band change). Wired
    `wifi_recon_set_band_scope()` and `channel_bitmap` passive-scan filtering
    in `wifi_recon.c`, preserved band scope across `wifi_recon_restore_shared_phy()`,
    and dispatched `OCP_VID_SET_BAND` / `OCP_VID_SET_CHANNELS` in `ocp_server.c`
    (refused with `code=busy` while any PHY tool runs).
  - **Deck (`firmware-cardputer`):** added `app/wifi_band_sync.h`, SD
    persistence (`loadWifiBand`/`saveWifiBand` in `storage/settings`),
    `ProbeInfo::band` parsing in `ocp_client`, Settings `b` key + band
    display in `settings_view`, `band` / `channels` debug commands, and
    `wifi_band`/`probe_band` fields in `dump`. Updated `DESIGN.md` §7.1.
- **Checks:** `check_protocol.sh` (including new ASan/UBSan `wifi_channels_test`,
  `wifi_band_sync_test`, `client_test` 64/64, updated `test_wifi_spectrum_teardown.py`,
  and `check_rx_only.py`), mutation proof (dropping the band check trips
  `wifi_channels_test`), and `build_firmware.sh` (both `esp32c5` and
  `esp32s3` + `check_deck_stack.py`, `runDebugCommand` 384/2048 B).
- **Not yet verified on hardware:** live `set_band`/`set_channels` round-trip
  on flashed probe + deck and the D-14 5 GHz DFS channel bench check.

## 2026-11-21 — Drive rows: one cheap card check per record (~8-10x faster)

- **Why the check exists:** on 2026-09-17 a card pulled mid-drive left
  `wardrive_open=1`. Arduino's `File::flush()` runs `fsync` but discards its
  result, so `print`/`flush` alone never noticed. The fix then was
  `SD.exists(session path)` after every append.
- **Why it was slow:** that stat walks the whole growing `wardrive/`
  folder, and ran four times per AP row (CSV row, close track, placemark,
  reopen track). Detection never needed the walk: every FatFs call whose
  result we can see first sends the card CMD13 (`disk_status`), and that is
  what fails on a pulled card.
- **Change:** each append only checks for a short `print`. Once per record,
  `commit()` flushes both files and stats the short fixed `/oscilla/wardrive`.
  `check_protocol.sh` fails if `SD.exists(path)` returns or a record loses
  its `commit()`. Both mutants (the HEAD file, and a track point without
  commit) trip it.
- **Measured on the bench card (32 drive sessions; throwaway benchmark
  build, real folder, 40 iterations × 3 runs; the benchmark was not
  committed):**
  - Stat of the session path: 26.5 ms. Stat of `/oscilla/wardrive`: 3.9 ms.
  - Real AP record (1 CSV + 3 KML appends): **124-166 ms → 15.3-15.9 ms**.
  - The bench GNSS has no fix, so no live rows were written. The real
    build was flashed afterwards; Drive opens in 0.40-0.50 s and closes
    cleanly.
- **Trade-off:** a crash mid-record can now lose up to one record's
  unflushed appends instead of the last append. `recoverOpenSession()`
  already repairs a cut-short KML tail.
- **Not yet verified — needs hands:** a card pull mid-drive must still close
  the session (`wardrive_open=0`, `SD: ABSENT`) and a later `wardrive` must
  remount, as on 2026-09-17. The detection path (a FatFs stat that sends
  CMD13) is unchanged in kind, but it has not been demonstrated with this
  build.

## 2026-11-21 — Wi-Fi region setting (D-19); Drive start no longer stalls

- **Wi-Fi region, default US.** New additive verb `set_region <CC>`
  (OCP-SPEC §10.8; `region=` in `[HELLO]`/`[STATUS]`). One table,
  `protocol/ocp_wifi_regions.h`, gives each region an ESP-IDF country plus
  the non-DFS channels that country's regulatory row allows: `US`/`CA`
  1-11, `MX`/`EU`/`GB` 1-13, `JP` 1-14, each with its allowed 5 GHz set.
  The probe sets the country (802.11d off, RAM storage) and every hopper
  re-reads the list at start. It refuses a change while the PHY lane is
  held and boots as `US`. The deck saves the choice on SD
  (`/oscilla/wifi_region`) and resends whenever the probe reports a
  different region and the lane is idle. Settings card: `,` `/` cycle the
  region, with state shown as APPLIED / PENDING / PROBE OFFLINE / PROBE TOO
  OLD. Debug: `region <CC>`, plus `wifi_region` / `probe_region` in `dump`.
- **Hardware (both boards on their own USB):**
  - Zero `hop to ch` errors over a live-discovery lap in `US`, `EU`
    (12/13), `JP` (14) and `MX`, against every lap failing 12/13 before.
  - `packet_monitor 13` gives `badarg` under `US` and is accepted under
    `EU`.
  - Each change applied in about 1.1 s. A change made during Wi-Fi live
    waited for `stop`.
  - `CA` survived a deck reset and was re-pushed after a probe reboot.
  - 0 link timeouts.
- **Host:**
  - `wifi_regions_test` is built as both C and C++; a mutant that puts
    12/13 into `US` fails it.
  - `wifi_region_sync_test` covers the push rules.
  - `client_test` covers `region=` and the verb (60 checks).
  - RX-only, palette, render-sync and OCP selftest gates pass; uart and
    bench probe builds pass.
- **Drive start (`ef446ea`):** 1.7 s → 0.40 s. The walk is readdir-only,
  and only the KML named by the open-session marker is repaired. 0.70 s
  after a mid-drive deck reset, including that repair. Not verified: the
  repaired KML's footer contents (needs the card on a host). Cards with a
  KML cut short before this build, and no Drive start since, are not
  repaired.
- **Not changed:** wardrive `appendVerified()` still runs `SD.exists()`
  after every row, as its card-pull health check. That is one directory
  lookup per row, growing with the folder. Changing it changes SD fault
  detection, so it is left for a deliberate decision.
- **Not yet seen by the operator:** the Settings card layout (no host
  renderer for deck LCD views). The external panel's settings strip still
  shows brightness only.

## 2026-11-21 — Probe `rst=` confirmed; LoRa start froze the deck for 6 s (fixed)

- **Probe flashed** with the `b0a060c` `build-uart` image (probe and deck each
  on their own USB). `rst=usb` after the flash's RTS reset, `rst=sw` after an
  OCP `reboot` (probe console shows the restart). `rst=poweron` still needs a
  physical power cycle.
- **The "LoRa start under Wi-Fi live always times out once" issue (2026-11-20)
  was a deck stall, not the probe.** With both consoles captured, `lora` →
  `lora log: recording` took 6.3 s with Wi-Fi off too, and the deck's GNSS
  UART2 overflowed meanwhile: the main loop was frozen. Cause:
  `loraLogBegin()` picked the next session number by probing `SD.exists()`
  on three paths per candidate index, from 1 up. Each probe is a FAT lookup,
  O(sessions²) overall on the bench card's accumulated soak/mash sessions.
  Any OCP reply pending across the freeze was counted as a timeout. My
  2026-11-20 guess (synchronous SX1262 start in the probe dispatch task) was
  wrong; the probe console shows RX up about 0.1 s after the verb.
- **Fix:** one `readdir` pass (`getNextFileName()`, parsed with the existing
  host-tested `model::sessionName`) marks used indices; then three lookups for
  the chosen index only, refusing rather than letting `FILE_WRITE` truncate an
  old session. First try used `openNextFile()` and was **worse** (~17 s): the
  Arduino VFS stats and opens every entry by path. `check_protocol.sh` now
  fails if per-index probing or `openNextFile(` returns to `lora_logger.cpp`;
  both mutants (the old loop, an `openNextFile` walk) trip it.
- **Hardware after:** `lora` → `recording` 1.60–1.61 s (the probe ack round
  trip) ×3 with Wi-Fi off and 1.60–1.70 s ×5 under Wi-Fi live, zero GNSS
  overflows, **0 link timeouts** (was one every time).
- **Same class, not fixed yet:** Drive start (`wardriveLogBegin`) blocks the
  loop ~1.7 s with 27 drive sessions on the card. `discoverSessions()` uses
  `openNextFile()` and re-runs `recoverKml()` on every KML each start, so it
  grows with the card.
- **Also seen on the probe console:** every Wi-Fi hopper's
  `esp_wifi_set_channel()` returns `ESP_ERR_INVALID_ARG` for ch 12 and 13
  (in `wifi_channels.c` but rejected by the default country), so those two
  channels are never covered by live discovery, sniffer, deauth or spectrum.
  This also answers part of D-14's bench question: a disallowed channel is
  rejected with a checkable error, not silently ignored.

## 2026-11-21 — Roadmap expanded with P7/P8 contract follow-ups and P9 enhancement phase

- Updated `ROADMAP.md` to capture the agreed firmware enhancement backlog while
  keeping v1 exit gates intact and every item strictly receive-only (D-8):
  - **P7:** explicit items for wiring up already-declared `set_band` /
    `set_channels` (plus the D-14 DFS bench check) and probe-driven multi-band
    `start_wardrive`.
  - **P8:** explicit items for decoupling synchronous `lora_listen` startup
    from the OCP dispatch task, persistent deck/probe settings (`settings` +
    `config.c`), D-UCB channel picking (D-6), LOGS sidecar inspection +
    session waypoints, and measured battery runtime slope.
  - **P9 (new, post-v1 additive phase):** richer 802.11 beacon/RSN parsing,
    multi-vendor BLE tracker classification, SX1262 sub-GHz RSSI energy sweep,
    SD-loaded LoRa profiles + CAD hopping, geotagged LoRa KML logging, passive
    baseline/anomaly watch mode, single-target Signal Hunt mode, offline
    SD OUI/BLE vendor tables, and post-handshake baud negotiation + bounded
    passive PCAP streaming (D-2).
- Doc-only change; no firmware or protocol files modified.

## 2026-11-20 — Probe reports its reset reason (`rst=`); probe flash pending

- Added `rst=` (OCP_K_RESET) to `[HELLO]` and `[STATUS]`: `poweron`, `sw`,
  `panic`, `int_wdt`, `task_wdt`, `wdt`, `brownout`, `ext`, `usb`, `jtag`,
  …, `unknown`. Additive key: OCP stays 1 and older decks ignore it. The
  deck stores it, logs `probe rst=` on each `Ready`, and exposes
  `probe_rst` in `dump`. OCP-SPEC §4 and the `status` description are
  updated.
- Why: a brownout, panic or watchdog restart was invisible from the deck,
  which slowed the stuck-probe hunt and matters for Grove-powered
  operation (D-4).
- Checks: host gate (client test 57/57, including parse and absent-key
  cases), both firmware builds, selftest 27/27, receive-only and stack
  guards. Mutant (parse line removed) fails the client test.
- Deck flashed and verified against the **unchanged** probe: link `ready`,
  `probe_rst` empty as designed. **Probe not flashed:** it is
  Grove-powered with USB disconnected. Pending: flash `build-uart`, then
  confirm `rst=poweron` after a power cycle and `rst=sw` after `reboot`.

## 2026-11-20 — Stability hunt: probe-reboot desync fixed; four tests clean

Live deck + Grove-powered probe, v0.3.0 plus the lane-reconcile fix.
Receive-only commands throughout. Harness kept outside the repo.

- **Probe heap fragmentation (50 Wi-Fi snapshots):** largest free block
  61,440 B after every snapshot, free 99,112–99,124 B, no errors. The
  earlier 77,824 → 61,440 B drop is a one-time settle after the first
  snapshot since boot, not a ratchet.
- **Probe reboot mid-session — bug found and fixed.** `reboot` during LoRa
  RX + Wi-Fi live: the probe was back idle in 3.9 s, but the deck kept
  `wifi_live=1`, `lora_active=1` and **its LoRa recording open** for a dead
  session, and `wifiscan` toggled "off" instead of starting. 3/3. The
  deck only cleared state for an *unsolicited* `[HELLO]` while `Ready`;
  a deck-commanded reboot, or a crash that drops the link first (boot
  3.9 s > 3 s ping), skipped it. **Fix:** on entering `Ready` the deck
  also requests `status`, and the first reply clears any lane the deck
  believes in but the probe reports idle (`owner=none`, `lora` not `rx`),
  through the same path as a `[STOP]` reply, so the recording closes.
  Missing keys never clear. Host tests extended and wiring guard widened;
  mutants (never clear PHY, no status on Ready) both caught. On hardware:
  3/3 consistent, `lora_open=0`, zero write errors, Wi-Fi restarts.
  Deck-reset regression from the first fix still passes.
- **Rapid mode switching (40 random bursts) and LoRa toggling (30):** no
  stuck state. Snapshot-mode mismatches self-healed within 3 s (the deck
  pauses status polling during snapshots, so the owner it shows lags);
  LoRa converged on a fresh status 30/30. `busy` errors from overlapping
  starts are expected. Start-immediately-after-reconnect race: 5/5
  consistent.
- **"1.21 s loop stall" corrected:** `max_cycle_us` is the external
  display's frame latency, rendered one strip per loop pass, not a
  blocked loop. Fresh-deck attribution: about 250–275 ms for most
  actions, 353 ms with LoRa SD logging, **819 ms with LoRa logging +
  Wi-Fi live**. The external panel refreshes about once a second in that
  mode; the keyboard and link keep running. Low priority.
- **Pre-existing, open:** starting LoRa while Wi-Fi live runs produces one
  reply timeout every time (`link_timeouts` +1; the link stays `Ready`
  and LoRa does start). Same on the fix-1-only image (`835e3e7`), so not
  introduced here. `lora_listen` starts the SX1262 synchronously in the
  probe's dispatch task; identifying the timed-out verb needs the probe
  USB console.

## 2026-11-20 — "Probe stuck after deck flash" found: deck/probe lane desync

- **Report (Will):** after flashing the deck without restarting the probe,
  the probe sometimes seems stuck until a full power cycle.
- **Ruled out on hardware** (probe Grove-powered from the deck, idle):
  10 RTS resets, 5 ROM download-mode entries and 3 real reflashes all
  reconnected in 4.4 s. No power dip, ROM-parking or UART-garbage hang
  reproduced. The probe's line assembler discards over-long lines and its
  idle read backs off, as designed.
- **Reproduced with a busy probe:** LoRa RX + Wi-Fi live, then reset the
  deck. The link came back `ready` in 5.2 s, but the probe still reported
  `probe_owner=wifi` and kept listening while the fresh deck believed both
  lanes idle. `lora config` and `wifiscan` then returned `err code=busy`,
  `lora` did nothing visible, and the UI had no running mode to stop. A
  power cycle "fixed" it only by making the probe forget too. `stop`
  cleared it at once. The same desync applies after a link drop, because
  the deck clears its BLE/spectrum/anti-surveillance state on any drop.
- **Fix:** on every transition into `Ready`, the deck stops each probe lane
  it does not believe is running (`app::reconcileStopLane`, one scoped
  `stop`, always acked per OCP-SPEC §6). Lanes the deck still owns are
  left alone, so a LoRa session it is still logging survives a link drop.
  Starts issued meanwhile queue behind the pending stop (PHY handoff) or
  follow it in order (LoRa).
- **Checks:** new `link_reconcile_test` plus a wiring guard in
  `check_protocol.sh`. Both mutants fail as expected (no stop on a fresh
  link: test aborts; call removed: guard fails). Host gate, deck build and
  stack guard pass.
- **Hardware-confirmed:** flashed the fix while the probe was busy. On
  reconnect `probe_owner=none`; `lora config` acked, `lora` started
  recording, `wifiscan` acked. Not exercised: a real mid-session Grove
  link drop with deck-owned LoRa (covered by the unit table only).
- Open diagnostic gap: the probe never reports its reset reason, so a
  brownout or watchdog restart is invisible from the deck.

## 2026-11-20 — v0.3.0 periodic use and two-hour soak PASSED

- Run `/tmp/oscilla-periodic-long-v030-20261120-164039`, controller reused
  unchanged from the interrupted 13:35 run with paths rewritten. Firmware:
  v0.3.0 on both boards, deck ELF `b4f2daa87c4b8b066f850f13d598d0e941057d1310e5d41506bb1757da113038`
  (`launch.json`). **Power:** deck on a direct host USB port
  (no hub); probe **Grove-powered from the deck** with its own USB
  disconnected — operator's choice, beyond the D-4 bench rule, recorded so
  this run is not read as equivalent to the earlier USB-powered ones.
- **Outcome `completed`, 10,879 s.** Preflight link/SD ready at 16 s; both
  link transitions happened before preflight (the port-open deck reset and
  reconnect), none afterwards. Deck uptime 10,878 s and probe uptime
  10,923 s at the end: no reset on either board. Zero UART/write/link
  errors, timeouts, link resets, probe resets, telemetry retries, deck
  panics, packet or health drops, LoRa hardware faults. 25 config acks
  (20-round regression + run). Minimum free loop stack 2,768 B (guard
  2,048 B).
- **Periodic use, 3,604 s, 6/6 cycles** (Wi-Fi live, BLE live, sniffer,
  packet monitor, BLE snapshot, Wi-Fi snapshot; LoRa RX + SD logging and UI
  activation each use period). Every recording closed. 31 UI review cycles.
  End-of-idle deck free stepped down only as caches filled (136,928 →
  119,588 → 103,528 B with 173 APs, 92 BLE, 77 clients), then held; flat
  within each idle. Snapshot cycles dipped the deck largest block (61,428 /
  59,380 B) and it recovered to 69,620 B each time. Deck heap low-water
  56,100 B, set during the Wi-Fi snapshot. Probe recovered to 99,124–
  99,384 B free after each cycle; its largest block fell 77,824 → 61,440 B
  after the Wi-Fi snapshot and stayed there.
- **Two-hour steady stage, 7,200 s** (passive Wi-Fi live + LoRa RX/SD
  logging, GNSS fixed). Deck free fell 125,908 → ~108 KB in the first four
  minutes as the AP cache refilled (102 → 172 APs), then held at
  105.9–108.6 KB to the end with largest block fixed at 69,620 B. Probe free
  constant at 94,684 B, low-water 59,344 B. 1,157 LoRa packets, 722 health
  rows, 2,421 writes / 312,610 B, zero drops. Note: the controller's UI
  review branch is inactive in this stage, so it covers steady receive and
  logging without key input, not continuous UI use.
- Worst single deck loop pass 1.21 s (`max_cycle_us` 1,214,657), set between
  idle 1 and the cycle-2 BLE-live switch and never exceeded; the earlier run peaked at
  0.95 s. Not a fault, but a UI-responsiveness lead worth a look.
- Cleanup confirmed: receive lanes stopped, both recordings closed, debug
  off. Earlier USB `-71` disconnect did not recur on a direct port, which
  supports (does not prove) the host-path diagnosis.
- Not covered: battery power, measured current/supply dips, BLE concurrent
  with Wi-Fi in the long stage. P6 exit gate (current budget + interlock)
  remains open.

## 2026-11-20 — v0.3.0 milestone cut; P5 closed

- First tagged version. Committed the uncommitted work since 2026-10-09 in
  five groups (hardware, deck firmware + tests, brand/preview tooling, soak
  tooling, docs), then the release commit. Process recorded in
  `docs/RELEASING.md`; notes in `CHANGELOG.md`.
- Version now comes from the repo-root `VERSION` file: the probe's CMake
  reads it into `PROJECT_VER`, and the deck's `version.py` pre-script defines
  `OSCILLA_DECK_VER`. Both builds confirmed to embed `0.3.0`. OCP stays at 1;
  `protocol/` and `firmware-c5/` sources are unchanged since 2026-10-09.
- Gates on the release tree: `check_protocol.sh` (ASan), `ocp_repl.py
  --selftest` (27/27), `build_firmware.sh` (both) and `--bench
  --probe-only`, `render_external_views.py --sync-guide-assets` (no diff),
  `check_ui_palette`, `check_rx_only`, `check_deck_stack`, card selection /
  sweep layout / session highlight tests. All passed.
- **P5 exit gate met**, on operator report (Will, 2026-11-20): repeated cold
  boots with SD inserted and a ~10 h USB-powered soak without bus corruption,
  alongside the earlier bounded concurrent redraw + disposable-file fixture.
  No captured log for the cold boots or soak. Operator also accepted the
  expanded external views as complete with Observatory v2.
- Second-panel detection dropped: a write-only panel cannot be detected, and
  the operator confirmed the project relies on the panel, so DESIGN §7.4 now
  calls it a standard fitment. Touch: no touch hardware or code.
- Added P8 item for idle power saving (display timeout on no key press, then
  a deeper sleep that keeps receive/logging alive), per operator.
- Flashed after the cut: deck `50:78:7d:ce:6d:64` (`cardputer-adv`, four
  hashes verified, clean boot at 182,232 B free heap, link cycling with
  probe unpowered) and probe `38:44:be:bf:d2:94` (`build-uart`, three
  hashes verified, boot log `App version: 0.3.0`, `probe up on uart0`).
  Paired-link `Ready` after reconnect not yet observed. P6
  periodic use, the two-hour new-image soak, battery soak and current budget
  remain open.

## 2026-11-20 — Periodic-use capture interrupted by USB disconnect; long soak unstarted

- Run `/tmp/oscilla-periodic-long-20261120-133559` stopped on USB EOF/readiness
  error, followed by write `EIO` during cleanup. One complete five-minute
  use/five-minute idle cycle passed; cycle 2 completed BLE live use and closed
  its recording, but the idle period was interrupted. Last sample 1,029.93 s
  overall, 965.70 s after the periodic-ready sample. No periodic phase-complete
  marker; the new-image two-hour steady stage never started. No duration from
  this interrupted run is added to another run's qualification.
- All valid timed samples before disconnect had zero UART/write/protocol
  errors, timeouts/resets, LoRa hardware faults, drops, panic counts or telemetry
  retries. Minimum free loop stack remained 2,768 B. No timed reset/panic banner
  was received; this does not establish what happened while USB was absent.
- Wi-Fi-use cycle 1 sampled deck free 130,612–147,892 B. Its idle samples
  stayed within 135,568–135,720 B with largest block 94,196 B; probe recovered
  to 99,172 B. BLE-use cycle 2 sampled deck free 118,816–131,456 B; partial
  idle stayed within 120,088–120,168 B with largest block 92,148 B and probe
  recovered to 99,184 B. Different cached inventories explain differing
  workloads; these partial observations do not prove a leak or recovery gate.
- Both use recordings had confirmed closure markers before the disconnect:
  cycle 1 54 packet / 31 health rows, cycle 2 60 packet / 32 health rows.
  Last telemetry: SD/link ready, both files closed, PHY owner none, LoRa idle,
  196 accepted writes / 25,813 bytes including startup recording. Cleanup after
  disconnect could not be confirmed. Eleven UI/session-review cycles completed.
- Host kernel log corroborates USB transport loss: at 14:28:31 PST the deck
  serial identity disconnected, then repeatedly re-enumerated with configuration
  and descriptor errors `-71`; kernel attempted port power-cycle before it
  reappeared at 14:28:34. A separate apple-mfi-fastcharge device disconnected
  at 14:28:30. The deck was present again at inspection. This supports a USB
  connection interruption but does not isolate cable, hub, host, power or deck
  as the cause, and is not a confirmed firmware panic.
- Preserved failed-run diagnostics/matching firmware, documented the boundary
  and removed the follow-up after reporting the terminal outcome. No silent
  restart, further flash, commit, radio operation or wiring/power change. USB
  stability needs checking before a fresh uninterrupted attempt. The completed
  mode-switching test remains passed; periodic use/new-image long soak and
  P5/P6/power/cold-boot/field gates remain open.

## 2026-11-20 — Remaining periodic-use and new-image long soak started

- User authorized continuing the remaining tests after the separate first-test
  pass. Reused the unchanged AP-block-storage firmware, ELF
  `38d5783d80fc829f04e5b3b85d735830f8e98c2c6d9165cfeb07dcda73abd66a`;
  no flash, wiring/power change or commit. Preflight confirmed ready link/SD,
  no existing recording and zero UART/write errors; no collector held the port.
- New detached run `/tmp/oscilla-periodic-long-20261120-133559` performs six
  five-minute use/five-minute idle cycles (passive Wi-Fi, BLE, sniffer, packet
  monitor and bounded snapshots, LoRa RX/SD logging and UI/session review), then
  a fresh uninterrupted two-hour passive Wi-Fi plus LoRa receive/logging soak.
  One exclusive connection remains open through both phases; final cleanup
  stops both receive lanes and closes recording. Count-only diagnostics and
  matching firmware artifacts stay under `/tmp`; no raw observations published.
- Periodic-only controller dry run passed six cycles and recording closure,
  without repeating the completed twelve-slot mode-switch test. Runtime guards
  and automatic reports remain unchanged; half-hour follow-ups inspect phase
  completions/faults without repeated polling. Setup is separate from timing.
- Live startup acknowledged twenty configuration commands, closed its warm-up
  recording and began periodic cycle 1. UART/write errors zero; minimum free
  loop stack 2,768 B. Current run is in progress; no periodic-use or new-image
  two-hour result is claimed yet. Prior first-test pass remains separate.

## 2026-11-20 — First mode-switching test passed, including closure and idle recovery

- First-test-only run `/tmp/oscilla-switch-only-20261120-123000` completed on
  ELF `38d5783d80fc829f04e5b3b85d735830f8e98c2c6d9165cfeb07dcda73abd66a`.
  All twelve 150-second mode slots completed in 1,800.43 s: passive Wi-Fi/BLE
  live/snapshot modes, both sniffer and packet-monitor slots, deauth detection,
  tracker watch, anti-surveillance and the previously failing final Wi-Fi slot.
  Independent LoRa RX/SD logging continued through switching. No transmit mode.
- Verified the JSONL records independently of the terminal outcome: all twelve
  sequential completion markers, both switching/recovery stage markers, 154
  timed samples, continuous deck/probe uptime and unchanged link-transition
  count. All timed UART/write/protocol errors, timeouts/resets, radio hardware
  faults, packet/health drops, deck panic counts and telemetry retries were zero.
  Minimum free loop stack was 2,816 B throughout timed samples.
- Sampled deck free heap across switching/closure/recovery ranged
  56,172–145,352 B; largest blocks 39,924–106,484 B. Boot-lifetime heap minimum
  ended at 46,784 B (a low watermark, not current free memory). Both sniffer
  slots and final Wi-Fi passed without allocation abort. These are bounded
  mixed-workload samples, not a zero-leak claim or full-capacity RF fixture.
- Recording closed with 321 packet / 182 health rows, 506 accepted SD writes
  / 67,149 bytes including setup. Confirmed LoRa/wardrive files closed, SD/link
  ready, PHY owner none and LoRa idle in closure/recovery/final telemetry. This
  confirms reported closure, not independent SD-file byte-integrity readback.
- Idle recovery spanned 120.13 s between its first and last samples. Deck free
  heap stayed within 99,376–99,464 B, largest block exactly 39,924 B. Probe free
  heap recovered from 94,732 B to 99,172 B; largest block stayed 65,536 B.
  Final free heap was deck 99,412 B / probe 99,172 B with cached inventories
  retained. No heap fit across different radio modes was treated as a leak trend.
- Collector completed cleanup, disabled RAM debug and closed its connection.
  Follow-up automation removed after verification/reporting. No commit, further
  flash, power/wiring change or automatic periodic/long test. The requested
  first-test finish line is demonstrated; periodic use and the new-image long
  soak remain unrun. P5/P6, measured-power, cold-boot and field gates stay open.

## 2026-11-20 — AP inventory growth in small blocks; first-test qualification only

- User narrowed the immediate finish line to getting the first mode-switch
  test through cleanly. Periodic use and the long soak will not automatically
  follow this run. Keep all previous failed-run evidence and start timing anew.
- The matching-ELF failure was contiguous AP-vector growth, not contact-snapshot
  refresh. Replaced AP row storage with `std::deque`: additions and frozen
  display copies allocate small blocks rather than a replacement table-sized
  block. Row order/indexing, the 512-row cap, paged continuation, full-table
  upserts, cached Stop behavior and frozen display semantics are unchanged.
- New constrained-allocation test fills all 512 AP rows, copies at growth
  boundaries and saturation, updates while frozen, stops/clears/restarts four
  times and fills all pages. Every allocation must fit within 4 KiB; largest
  observed host allocation is 2,544 B. The original contiguous implementation
  fails this test, including when reintroduced on an isolated header copy.
  Integrated into the full host suite. This bounds tested allocation sizes,
  not total memory use or physical end-to-end endurance.
- Prepared a first-test-only collector: twelve 150-second passive mode slots,
  clean recording closure, then two minutes of idle recovery. Its mocked
  controller passes all slots/closure/recovery and confirms no periodic-use
  phase starts. Diagnostic panic counts now include `abort()` before aborting
  collection. Full host/protocol/RX-only checks passed with the documented
  `ASAN_OPTIONS=detect_leaks=0`; both normal firmware builds passed. Compiled
  dispatcher 192 B, System renderer 256 B, main loop 960 B; all stack budgets
  pass (debug dispatcher now 384 B).
- Preflash console writes/replies initially failed with a partial command/raw
  framing mismatch. Retried without toggling modem controls using raw mode,
  a leading newline and per-reply deadlines. Confirmed link/SD ready, both
  recording files closed, UART/write errors zero. Missing/incomplete telemetry
  was not treated as proof of SD absence or a board reset.
- Flashed normal ESP32-S3 deck application only; stable MAC
  `50:78:7D:CE:6D:64` and flash hash verified. New ELF SHA256
  `38d5783d80fc829f04e5b3b85d735830f8e98c2c6d9165cfeb07dcda73abd66a`.
  Matching ELF/bin, count-only telemetry and launch metadata are saved under
  `/tmp/oscilla-switch-only-20261120-123000`; `/tmp/oscilla-soak-current` points
  there. Probe firmware/power/wiring unchanged. No commit made.
- First-test-only collector is running on one exclusive USB connection. The
  first Wi-Fi slot completed; BLE snapshot slot samples report zero UART/write
  errors and 2,816 B minimum free loop stack. Automated follow-up checks run
  sparsely. No full first-test pass, periodic-use or new long-soak result is
  claimed while the run remains active.

## 2026-11-20 — Memory-fix retest: sniffer slots passed, final Wi-Fi allocation abort

- Run `/tmp/oscilla-mode-memory-retest-20261120-073435` ended in a detected
  allocation abort at 1,712.19 s overall. Eleven 150-second switching slots
  completed; the final Wi-Fi live slot began but did not finish. Thus the
  30-minute switching gate still fails. Periodic use and the new-firmware
  two-hour steady phase never started; no idle-recovery or steady-fit result
  exists for this image. The previous image's two-hour pass remains separate.
- Both sniffer slots completed without the earlier contact-vector abort.
  Last deck free heap was 93,228 B after the first and 85,500 B after the
  second. Across both sniffer slots sampled free heap never fell below 84,972 B
  and largest blocks stayed at least 45,044 B, versus the earlier failure's
  38,476 B free / 7,796 B largest. Ambient inventory differs, so this is useful
  partial hardware evidence, not proof that every saturated table fits.
- Final sample at 1,704.30 s: deck free 78,884 B, largest block 33,780 B,
  minimum free loop stack 2,800 B, 44 APs, 57 clients / 31 requests and 96 BLE
  devices retained. Before the abort all sampled UART/write/protocol faults,
  timeouts/resets, LoRa hardware faults and packet/health drops remained zero,
  and telemetry needed no retry. LoRa had 331 packet / 165 health rows and SD
  499 accepted writes / 65,648 bytes including setup. GNSS was fixed at the
  final sample. These counters do not imply clean recording closure.
- Captured ELF prefix `94836493e2817026` matches the current full ELF SHA256.
  The decoded abort is now `operator new` / vector reallocation of `ApRow`
  in `ScanModel::absorbEvent` (`scan_model.cpp:199`), through the Wi-Fi event
  dispatch path. The remaining failure is live AP-vector growth under retained
  mixed-mode inventories/fragmentation, not the old contact refresh allocation.
  Last-sample free bytes cannot establish allocation-time headroom or the exact
  requested block size. The preceding BLE-live slot had a largest-block minimum
  of 18,420 B, and Wi-Fi snapshot held 26,612 B largest blocks.
- Collector stopped on the abort and captured reboot diagnostics; cleanup and
  recording closure could not be confirmed. It did not automatically restart.
  No further flash, radio operation, power/wiring change or commit performed.
  Removed the retest follow-up after documenting/reporting this terminal result.
  Next work must bound AP/event-table growth and mixed-mode allocation peaks,
  then repeat switching before periodic-use qualification. Power, cold-boot,
  P5/P6 and physical/field gates remain open.

## 2026-11-20 — Mode-switch memory pressure: snapshot overlap and inactive display copies

- Read overnight telemetry with `os.open` and no DTR/RTS changes; HUPCL was
  already disabled. No reset banner appeared. Deck uptime 36,705 s and probe
  uptime 44,397,263 ms, link/SD ready, no recording open, no reported UART/write
  errors, deck free 182,704 B / largest 159,732 B. This is post-crash idle uptime,
  not periodic-use or overnight active-load qualification.
- Matching-ELF diagnosis from the failed run remains allocation failure in
  `ContactsModel::absorbClients`. Source review found two contributing peaks:
  contact/BLE refresh constructed a new growing vector while retaining the old
  table, and the strip renderer retained full copies of models from screens
  already left. Wi-Fi → BLE → sniffer therefore accumulated display-owned copies
  in addition to the authoritative app caches and incoming wire frame.
- Contact/request/advertiser snapshot replacement now reuses existing slots.
  When storage must grow, it releases the old table first and reserves only the
  bounded incoming row count. Validation, row caps, selection remapping, snapshot
  ages and Stop's cached-result semantics are preserved.
- On external-view changes, discard the previous display-owned copies before
  sampling the next view. Placement construction avoids large stack temporaries;
  authoritative receiver/cache models are untouched. In-progress strips still
  render a frozen model, and physical-row invalidation remains in place.
- Added constrained-allocation refresh/growth tests and an actual-dispatcher
  host test covering mid-frame Wi-Fi/BLE/sniffer transitions, frame coherence,
  released vector capacity and preserved source caches. Both regressions failed
  before the corresponding fix, passed afterward, and failed again with the
  old allocation/lifetime behavior reintroduced on disposable copies. Integrated
  both into `tools/check_protocol.sh`. Full host/protocol/RX-only suite passed
  with the documented `ASAN_OPTIONS=detect_leaks=0`; standalone OCP conformance
  passed 27/27. Compiled dispatcher remains 192 B, System renderer 256 B,
  main loop 960 B; all stack guards passed.
- Saved matching pre-fix ELF/bin under `/tmp/oscilla-before-mode-memory-fix.*`.
  No raw observations added to the repository. The earlier periodic-use phase
  never started; it has no prior failure/result independent of the switch abort.
- Initial broad build also tried every diagnostic environment: both normal deck
  targets and probe passed, but `sd-display-check` / its ST7789 alias have stale
  source filters missing session/dossier renderer definitions and failed linking.
  These diagnostic filters remain unresolved. Scoped the standard dual-firmware
  builder to `pio run -e cardputer-adv`; its real-board builds and stack guard
  now pass end to end. No benchmark binary was flashed.
- Flashed normal deck application only at `0x10000`, explicit ESP32-S3 and
  stable serial identity; esptool confirmed MAC `50:78:7D:CE:6D:64` and data hash.
  New ELF SHA256 `94836493e2817026ec2ee92e59d4869f23ab1ab70b364a1d8133429af4e93514`.
  Probe firmware was not flashed. No recording was open before the update.
- Detached retest under `/tmp/oscilla-mode-memory-retest-20261120-073435` runs
  30-minute passive switching first, then six five-minute use/five-minute idle
  cycles, then a fresh two-hour steady soak. Same held USB connection, bounded
  telemetry deadlines, automatic count-only reports and half-hour follow-ups.
  Live preflight: link/SD ready, UART/write errors zero. Timed qualification is
  in progress; new-firmware phase completion is not yet claimed.

## 2026-11-19 — Two-hour steady soak completed; mode switching exposed allocation abort

- The scripted steady workload completed 7,200.50 s uninterrupted, with passive
  Wi-Fi discovery, independent LoRa receive/SD logging and paired displays.
  Deck/probe uptime stayed continuous. All sampled UART/write/protocol errors,
  link timeouts/resets, radio hardware faults and packet/health drops remained
  zero; no telemetry retry. GNSS acquired a fix after the initial no-fix baseline.
  The steady endpoint had 1,444 packet rows / 723 health rows and 2,170 SD writes
  / 296,462 bytes including setup. Recording stayed open for mode switching.
- Excluding the first ten minutes, deck free heap ranged 107,676–111,272 B,
  ended 109,520 B and fitted -14.185 B/min. Largest block ranged
  92,148–96,244 B and ended 96,244 B. Probe current free heap was exactly
  94,944 B and largest block exactly 77,824 B at every steady sample. Minimum
  free loop stack stayed 2,800 B. This demonstrates bounded memory under this
  steady workload; the small fitted deck decline is not a zero-leak proof.
- Mode-switching completed the first Wi-Fi and BLE-snapshot slots, then the
  deck aborted in the third (sniffer) slot at 7,686.87 s overall, 437.52 s
  (7 min 17.52 s) after the steady phase completed. Last sample: 38,476 B free,
  largest block 7,796 B, 62 clients / 22 probes with 96 BLE devices retained.
  Probe free heap stayed 94,944 B; reported transport/storage faults were still
  zero. The 30-minute switching phase failed and periodic use never started.
- Panic ELF prefix `661de21777b2f0f0` matches the local ELF SHA256. Decoded
  trace reaches `operator new` / `__cxa_throw` / `std::terminate` through
  `std::vector<ClientRow>` reallocation in `ContactsModel::absorbClients`
  (`contacts_model.cpp:113`), then contact snapshot / OCP receive dispatch.
  This identifies allocation failure during snapshot replacement, distinct
  from the earlier fixed debug-command stack panic. Retained inventories and
  overlapping snapshot allocations need a memory-budget fix; these samples
  alone do not establish an unbounded leak.
- Collector detected the abort and stopped; reboot diagnostics were captured
  on the same connection. Cleanup/recording closure could not be confirmed.
  No automatic restart, firmware change/flash or commit. Count-only evidence
  remains under `/tmp/oscilla-soak-20261119-185748`; follow-up automation removed
  after reporting the terminal outcome. P5/P6, measured-power, cold-boot and
  physical/field gates remain open. Next qualification must address this
  allocation failure and repeat switching before periodic-use evidence exists.

## 2026-11-19 — Scripted endurance continuation and sparse follow-up

- Recovered the November 17 collector outcome: USB read disconnected after
  the last sample at 1,040.70 s overall (990.38 s after loaded baseline).
  Cleanup could not be confirmed. None of the two-hour, scan-switching or
  periodic-use phase gates completed; the earlier in-progress entry is not
  a completed endurance result.
- Resumed the authorized plan: two hours passive Wi-Fi plus independent LoRa
  receive/SD logging, 30 minutes passive scan switching, then six five-minute
  use/five-minute idle cycles. The collector owns one exclusive USB connection,
  saves counts only under `/tmp`, and stops on reset, stale probe telemetry,
  protocol/storage faults, unexpected workload changes or low stack margin.
  Separate 802.15.4 stop/reboot qualification remains outside this sequence.
- Initial preflight found no probe link; user restored the probe. Live
  preflight then confirmed ready link, SD ready, no existing recording, zero
  UART/write errors. No firmware flash, power/wiring change or commit performed
  by the agent. Twenty configuration acknowledgments passed; the new steady
  timer started after setup at 48.85 s overall. Baseline: deck free 120,804 B,
  probe free 94,944 B, largest blocks 98,292 / 77,824 B, free loop stack minimum
  2,800 B. Wi-Fi and LoRa/SD recording active, UART/write errors zero. GNSS
  had no fix at baseline; GNSS/moving-route qualification is not implied.
  Timed phases are still in progress at this entry.
- Added `tools/soak_report.py` for atomic count-only reports: explicit terminal
  outcome and completed stages, latest telemetry, largest blocks, minimum free
  stack and steady-workload heap fits excluding ten minutes of warm-up. Four
  synthetic tests pass; deliberately including warm-up on a disposable copy
  fails the regression. Existing follow-up controller dry run confirms twelve
  mode slots and six use/idle cycles with recording closure.
- Detached collector and half-hour thread follow-ups replace repeated polling.
  `/tmp/oscilla-soak-current` identifies the run directory containing
  `suite.py`, `phases.py`, `pid`, `progress.log`, `samples.jsonl`, `report.json`
  and optional `stop` request file. Reports are observations, not automatic
  leak verdicts. No endurance or P5/P6 exit gate is claimed yet.

## 2026-11-17 — Endurance setup exposed a debug-command stack panic

- User authorized an uninterrupted two-hour run, 30 minutes switching passive
  scan modes, then powered-on periodic use. Periodic plan is six cycles of
  five minutes use / five minutes idle; 802.15.4 stop/reboot qualification stays
  separate because of the documented C5 coexistence recovery behavior.
- First long-run setup detected deck uptime falling from 4,089 s to 16 s and
  stopped; no endurance duration is claimed. User confirmed no physical reset
  and suggested the serial monitor as a possible cause.
- Held-open exclusive serial capture distinguished two events: opening the
  native USB port produced ROM `rst:0x15 (USB_UART_CHIP_RESET)`, then the LoRa
  configuration debug command produced a separate Core 1 unhandled-debug panic
  and `rst:0xc (RTC_SW_CPU_RST)` while the connection remained open. No other
  serial-monitor process held the port during inspection. The precise cause of
  the initial uncontrolled attempt cannot be reconstructed from uptime alone.
- Panic ELF SHA 80d4eb9ecd3c70c6 matched the embedded SHA in the saved pre-fix
  image. Symbolication reaches vsnprintf / to_string -> sendLoraConfig ->
  startLoraConfig -> runDebugCommand -> console LineReader -> loopTask. The
  dispatcher reserved a 4,912-byte frame, largely from the long diagnostic
  concatenation, even when handling a small command.
- Moved the unchanged 63-field count/state snapshot into its own small-frame
  member and serialized fields sequentially with a framework-neutral builder.
  Number bounds and single-line text sanitization are host-tested. No captured
  names/identities/payloads added, no protocol or radio-capability change.
- Compiled frame regression rejects the saved old ELF, then passes the fix:
  dispatcher 368 bytes, snapshot 224, LoRa sender 448 and main loop 960. Guard
  runs from the normal dual-firmware build script. Per-frame budgets complement
  rather than prove full call-chain stack safety. Added `stack_free_min` bytes
  to RAM debug memory telemetry; installed ESP-IDF FreeRTOS defines that unit.
- Initial fixed-device regression acknowledged 20 configuration commands with
  no panic. New stack telemetry exposed a separate low-margin path: System
  rendering left only 1,168 bytes on the default loop stack. Compiled frames
  were 2,192 bytes in external dispatch and 2,176 in System rendering.
- Removed the large fallback snapshot temporary and streamed heap plot points
  from the frozen history rather than materializing two 61-point arrays on
  stack. Preserved all scaling/gap/restart/wrap behavior. New host visitor test
  failed before implementation, then passed. Active/cached/empty System source
  renders are byte-identical; all 130 paired fixtures render and highlight
  regression passes. Compiled dispatch is now 192 bytes; System renderer 256.
  Extended the stack guard to both renderer frames. No loop-stack size increase.
- Added numeric transport counters, receiver ownership/state and probe uptime
  to the same count-only snapshot for precise mode/idle qualification. Final
  snapshot frame is 240 bytes; other guarded frames remain as above. Full host
  checks pass; System fixture pixels remain identical, and Cardputer builds.
- Final device regression acknowledged 20 LoRa configuration commands mixed
  with dumps on one held-open connection with no panic or uptime discontinuity.
  Minimum free loop stack was 3,648 bytes after that regression, 2,768 after
  saved-session warm-up. The new two-hour timer began after setup; subsequent
  scan-switch and powered-on periodic phases use the same connection. This
  sequence remains in progress; no long-duration qualification is claimed yet.

## 2026-11-17 — Targeted heap soak: bounded after warm-up

- Ran the unchanged flashed firmware on the attached USB bench: continuous
  passive Wi-Fi discovery, independent MCORE LoRa receive/SD packet and health
  logging, GNSS and both displays. Exercised saved-session summary/record paging
  and System detours 24 times, including two warm-up cycles. No transmit mode,
  firmware flash or wiring/power change. Only count/state telemetry saved on host.
- Collected 123 active-load samples covering 1,817.10 s (30 min 17 s) across two
  recording segments: 616.67 s then 1,200.43 s. The first collector's fixed USB
  read windows missed a complete telemetry batch during review and cleanly
  stopped its recording. Cause not isolated; this is a measurement interruption,
  not evidence of a board reset. Resumed with per-reply five-second deadlines
  and one bounded retry; the second segment completed with zero retries.
  The chart marks the interruption; this is not an uninterrupted 30-minute run.
- Deck free heap initially fell while AP inventory and the bounded packet cache
  filled. Reopening LoRa after the sampler interruption clears/repopulates that
  packet cache. After review cycling ended, all 41 recovery samples were within
  118,968–120,112 bytes, ending at 119,996; last-five-minute median 119,654.
  Recovery endpoint rose 1,028 bytes from its first sample. The last-five-minute
  straight-line fit was -44.52 bytes/min amid the bounded variation; this pass
  cannot bound smaller leaks over hours. No clear sustained loss from the
  repeated session reviews was demonstrated.
- Probe free heap was exactly 94,788 bytes at every active sample. Largest free
  blocks were unchanged at every active sample: deck 102,388, probe 65,536.
  Final lifetime minimums were deck 111,484, probe 65,028; those are boot-lifetime
  low watermarks, not present free memory or proof of a leak.
- Two recordings saved 326 LoRa packets and 191 health rows. SD accepted 523
  writes / 70,253 bytes. Zero packet/health log drops, write errors, UART errors,
  reported radio hardware faults, observed probe resets or link-state changes.
  GNSS fixed in all 123 active samples; final cumulative checksum/overlong counts
  both zero. Session external-frame high 313,826 us, final cycle 40,342 us; no
  sustained frame-rate qualification is inferred from those cycle counters.
- Both test recordings closed cleanly. Final free heap after close: deck 120,828,
  probe 99,228. System left open with Wi-Fi receiving; RAM debug disabled. Raw
  records remain on the device SD only; host diagnostics/chart are in /tmp.
- Result: useful heap/regression evidence for this workload, no clear ongoing
  leak found. Multi-hour uninterrupted soak, BLE/mode-switch stress, reset/
  reconnect recovery, measured current budget and final-harness/battery/field
  qualification remain open; P5/P6 exit gates are not rounded up.

## 2026-11-17 — External session highlight background corrected

- Operator photo showed black glyph-cell rectangles interrupting the filled
  green selected-session row. The shared instrument text helper always supplied
  the void background, overriding the row fill under both name and type labels.
- Added an explicit optional text background and supplied the row color to both
  labels. Other instrument labels keep their existing default. Physical-row
  tile hashing, frame sampling and SPI locking are unchanged.
- Pixel regression failed on the original source-rendered catalog, then passed
  with continuous highlight fill under both labels and normal unselected text.
  Integrated the check into the source-render tool. All 130 paired fixtures
  rendered; corrected catalog visually reviewed. Cardputer ADV build passed.
- Preflash attached deck: SD ready, no recording files open, zero write/UART
  errors. ESP32-S3 upload verified deck MAC and flash hashes. LOGS reopened
  with 64 entries and Wi-Fi receiving; SD still reports zero writes and
  write/UART errors, RAM debug disabled. Operator confirmation of the physical
  highlight was subsequently confirmed fixed by the operator.
- Follow-up source audit covered the remaining external instruments, dossiers,
  meters and shared drawing helpers. No other text-over-filled-highlight
  mismatch found: other colored selection shapes keep labels outside their
  fills. Core onboard Wi-Fi/BLE/Sniffer/LoRa/802.15.4/Link list rows already
  supply the matching selected background. No further firmware edit required.

## 2026-11-17 — On-device saved-session review

- User prioritized offline session review while the display/probe harness is
  assembled. Added LOGS after Drive and `o` from live tools, preserving the
  receiver and nested System return. Enter drills into summaries, six-record
  pages and saved LoRa raw hex/ASCII; Back reverses one level. `x` filters
  All/Drive/LoRa; `r` refreshes inventory.
- Grouped writer-generated CSV/KML/manifest/health names only, at most 64
  sessions with explicit inventory limits. Type filters expose each family
  independently. Sidecar availability is shown; KML track rendering and
  metadata/health content parsing are deferred.
- Read-only SD service uses the existing SPI lock, one directory entry or at
  most 512 bytes per turn, parsing after unlock. It freezes CSV length on
  open and ignores later append activity. Shrinking/unavailable files,
  unrecognized headers, malformed rows and incomplete tails are explicit.
  No archive action writes/deletes/repairs a recording or starts a radio.
- Pure parser retains at most six rows and 32 signal samples; summary count
  excludes invalid/unfinished records. The graph is by record order, and
  observed span is first-to-last valid record time, not recording lifetime.
  Saved payload bytes stay raw and classification stays labelled a guess.
- Test-first parser initially failed for the missing header; then passed
  strict-name, grouped-inventory, hostile/chunked CSV, truncated tail, paging
  and Back cases. The mock filesystem enforces lock ownership, read-only
  mode and per-turn budgets; growth, shrink, contention, timeout, cancellation
  and missing-card checks pass. Hex-length and read-budget copy mutations fail.
- Offline controller test exposed missing redraw invalidation on Enter with
  no receiver running. Fixed explicit dirty state; controller test and a
  reverted-copy mutation prove it catches the bug. Navigation route test was
  updated for LOGS as the new final home card. Paired source previews reviewed.
- Before firmware upload, attached deck reported SD ready, no open LoRa or
  wardrive files, zero UART/write errors. Existing recording files preserved.
  First live test read 778 wardrive records with zero malformed rows, verified
  next/previous pages and System return while Wi-Fi kept receiving. It exposed
  an incorrect parser assumption: the writer stores LoRa CR as 1..4, not a
  5..8 denominator. Rejected rows were surfaced instead of silently shown.
  Corrected the range and added actual LoRa-writer round trips for all four
  CR values, plus a wardrive-writer escape round trip. These pass. Also cleared
  the error row background so text never overlays the summary status line.
- Corrected final build passed: static RAM 65,336 bytes, flash 804,973 bytes.
  Explicit ESP32-S3 upload verified deck MAC 50:78:7D:CE:6D:64 and flash hashes.
  Full host protocol/RX-only suite passed, including parser/reader/controller
  tests, actual writer round trips and all 27 OCP checks. 130 synthetic paired
  source-render fixtures generated; populated, loading, empty and error views
  reviewed, including clear error text and saved-hex Back labels.
- Final live test loaded 778 wardrive records and 46 LoRa records with zero
  malformed rows. Cross-page forward/back selection, saved LoRa hex/ASCII,
  nested Back, System detour/return and return to the live tool all passed.
  Wi-Fi remained active through review. Initial final-test setup had no deck
  live flag after the deck-only reset; a scoped PHY stop before starting the
  test receiver established the intended precondition. Reset recovery remains
  its own qualification gate, not claimed by this browser test.
- Read-only review reported zero SD writes and write/UART errors, GNSS fixed,
  no open recording files. Heap free 118,228, minimum 98,156, largest block
  94,196 bytes. Last external cycle 40,480 us, session maximum 275,855 us.
  This bounded navigation/read test does not establish a multi-hour soak gate.
  LOGS left open on LoRa sessions with Wi-Fi receiving and RAM debug disabled;
  operator physical readability/animation review remains pending.

## 2026-11-17 — Paired Wi-Fi AP and BLE advertiser dossiers

- Enter opens a pinned BSSID/MAC on both screens without stopping continuous
  reception. Back restores the same identity when present; missing rows retain
  their explicitly marked last observation. System detours preserve the receiver.
- AP radio details retain full identity, security, beacon/MFP provenance and
  age. `x` opens the observed-client radar from the aged Sniffer cache; `;`/`.`
  focus peers. No background Sniffer start or inferred bearing/association.
  `r` explicitly requests a passive survey, then beacon capture if the AP is
  still in the completed survey. Replies must match BSSID, not reused row index.
- BLE shows name, MAC, raw company identifier, tracker classification and source
  count. Its 16-point/120-second RSSI plot appends actual accepted observations
  only; first-sighting discovery, redraws and other devices cannot invent samples.
  `r` rescans, cached history freezes, and long gaps stay disconnected.
- New framework-independent inspection/history models and paired dossier views
  replace the old AP trace renderer. External frames retain the 200 ms model
  sampling boundary and the shared SPI/tile-diff renderer.
- Adversarial test initially failed for the absent inspection header. Identity
  reorder, foreign reply, missing row, deduplication, client selection, malformed
  RSSI, ring bounds, gaps and clock-wrap tests now pass. Copy-only mutations of
  the BSSID guard and sample deduplication both fail the test as required.
- Rendered actual C++ paired previews checked with synthetic identities. Initial
  build missed the new inspection include; initial host integration missed the
  ContactsModel link dependency. Both were corrected.
- Complete host protocol/RX-only suite passed, including all 27 OCP checks,
  plus the new dossier tests and two rejected mutations. Cardputer ADV build
  passed: static RAM 64,224 bytes, flash 779,733 bytes. Explicit ESP32-S3
  upload verified deck MAC 50:78:7D:CE:6D:64 and all flash hashes.
- Live receive-only input checks passed: AP Enter/Back kept continuous reception,
  System return preserved both dossiers, AP survey/capture returned valid beacon
  data, and BLE rescan retained its identity and grew history from one to two
  real samples. Other-device discoveries/redraws left history at one sample.
  BLE pause/Back passed. AP dossier left receiving; RAM debug disabled, no
  recording files open, SD mounted, no write/UART errors reported.
- 102 synthetic source-render fixtures generated. Both display layouts reviewed
  in source renders; user subsequently accepted the normal paired appearance.
  Remaining populated/empty/error states still need broader physical review.
  Latest external cycle 41,094 us, session maximum 245,093 us; this short input
  run does not establish a sustained frame-rate or combined-soak gate.

## 2026-11-17 — Stationary GNSS/wardrive comparison passed with test antenna

- User swapped to a larger active antenna for indoor testing, explicitly
  not the eventual case antenna. Receiver acquired a fresh fix with clean
  UART/NMEA counters. User confirmed antenna would remain stationary.
- Idle baseline: 64.03 s, 65 distinct fix epochs, all 12 status polls fresh;
  3.80 m horizontal RMS scatter around the sample mean, 29.86 m accumulated
  raw path (27.98 m/min) and 8.80 m first-to-last offset. HDOP polls ranged
  2.3–5.3. No recording/file writes during the baseline.
- Normal wardrive logging: 121.15 s, 122 distinct epochs, all 24 status
  polls fresh; 3.01 m RMS scatter, 39.94 m accumulated raw path (19.78 m/min)
  and 9.95 m first-to-last offset. HDOP ranged 2.3–5.5, so the difference in
  scatter is not evidence that logging improves positioning. These metrics
  measure repeatability/wander, not absolute positioning accuracy.
- Saved 778 AP observations, zero no-fix skips, zero new UART errors in
  either interval, zero checksum/overlong failures and zero reported write
  errors. Deferred close completed and files are closed; relative sampler
  and RAM debug mode are off. No coordinates, AP identities or payloads
  were copied into repository artifacts.
- Longest observed external cycle was 1,885,497 us across session startup/
  logging/closure, versus the earlier 14,134,041 us session high. This is
  bounded-run evidence, not a sustained animation-rate guarantee. Final
  heap free 154,120 bytes, minimum 116,596, largest block 102,388.
- Cooperative writer fix passed this stationary, fix-valid loaded test.
  Raw path accumulates substantial distance while physically stationary,
  including with logging disabled; it remains explicitly labelled RAW GNSS
  PATH, not travel distance. Outdoor/final-antenna qualification, moving
  route behavior and longer combined soak remain open.

## 2026-11-17 — Wardrive stall found; stationary drift measurement parked

- User accepted the Sniffer visuals, then requested stationary wardrive/GNSS
  testing. Antenna placement was beside a window. Baseline live recording
  showed GNSS UART errors 2 and 3 (Arduino buffer-full/FIFO-overflow), a
  14,134,041 us maximum external frame cycle, and a 13,899 ms fix age/no-data
  report. A later fix report does not erase that loss. This establishes a
  software service/latency problem alongside possible receiver position noise.
- Cleanly closed the existing recording before flashing; files preserved.
  Source showed a whole scan page synchronously appended/flushed from the
  OCP callback, with immediate next-page/rescan scheduling. Changed to one
  AP record per main-loop iteration, with page-receipt fix/time provenance
  captured before deferral. Page fetch/rescan waits for the drain. Protocol
  timeout timing excludes deliberate queued-write waits, with a fresh wait
  deadline when each page request is sent. No raw fix smoothing was added.
- A stop request drains queued rows before file closure; both displays show
  CLOSING. GNSS input is serviced before Grove/tick/AP-write work, track points require
  a fresh fix, and stale page fixes cannot be written as valid observations.
  Display distance is labelled RAW GNSS PATH, since summing raw fix segments
  also accumulates stationary wander. AP/track file formats are unchanged.
- Added RAM-only `gnss_test start|status|stop` repeatability diagnostics:
  relative offsets, RMS scatter, raw path, largest step, HDOP/fix age and
  parser counters; no absolute-coordinate output. UTC epoch deduplication
  avoids counting GGA/RMC reports of the same epoch twice. UART error counts
  are atomic and available in display_status.
- Test-first drain check rejected the original all-rows-per-call behavior.
  Actual extracted request/drain methods now pass one-row-per-tick, repeated
  tables, paging, rejected sends, deferred rescan and drain-before-close;
  cursor and premature-rescan mutations are rejected. GNSS diagnostic tests
  cover duplicate epochs, stationary return/path inflation, gaps, reset,
  invalid positions and dateline wrap; a dedup-removal mutation is rejected.
  Full host/protocol/RX-only checks and the Cardputer build passed. Flash
  hash verified on ESP32-S3 MAC 50:78:7d:ce:6d:64 (63,560 bytes static RAM,
  765,521 bytes flash). Shared SD/TFT lock/clock and receive-only limits remain.
- Receiver reported fix lost before the flash and searching afterward.
  A 90-second attempt yielded no usable position epochs, with zero checksum
  or overlong failures. Those zero-sample offsets/scatter are NOT a measured
  zero drift. User repositioned the antenna but it still did not acquire a
  fix, then explicitly chose to return to the test later. No stationary
  baseline or fix-valid heavy logging comparison is claimed. Writer-fix
  physical qualification, drift quantification and moving-route checks remain
  open. Cleanup confirmed: sampler inactive with zero samples, no recording
  open, debug disabled, SD ready, zero write/UART errors since this flash.
  The idle/no-fix counters do not qualify the fix-valid logging load.

## 2026-11-17 — Paired Sniffer clusters and live client dossier

- User approved Sniffer as the next polish pass. Reworked the companion
  into a selected AP/request cluster with prominent pink edge, selected
  client halo, full AP identifier, channel/band, exact RSSI/packet/age dock
  and explicit drawn/total counts. Root radii show RSSI measured at the
  probe; client-centred dossier AP/SSID nodes use identity-only orbit layout.
- Enter opens a paired client/request dossier without stopping reception.
  Composite MAC+BSSID or MAC+SSID identity pins both list selection and
  dossier updates across reordered snapshots. Missing rows retain their
  last observation/time, show ROW MISSING and use a dashed retained link;
  Back restores the selected row when present, otherwise clamps safely.
  Restart/pause, System inspection/return and probe-reset cleanup remain
  explicit. Graphs cap at 12 nodes and always retain the selected row.
- Onboard lists now fit the full 17-character MAC before their fixed
  channel/band/RSSI columns. Kept LAST ticker and added Enter/tab hints.
  The dossier shows full target identity, optional labelled cached AP scan
  name, channel/band, RSSI, packets and snapshot age. Unknown hop channel is
  -- rather than a fabricated channel 0. Extracted the existing safe ASCII
  display helper so real onboard source/chrome can share preview wiring.
- Added test-first identity/reordering/removal/wildcard/cap fixtures. Copied
  identity-following and overlapping-column regressions were rejected.
  The first full check caught removal of the LAST label; restored the ticker
  alongside the action hint and strengthened actual column-width checks.
  Tight graph scans compare referenced strings without per-row heap copies.
- Source-render QA now includes actual 240x135 onboard contacts/dossier and
  chrome, alongside the external renderer, plus crowded, multi-link,
  missing-row and cached-name cases. These are synthetic fixtures, not
  framebuffer captures. Added a RAM-debug-only single-key bench route
  through the existing onKeys handler and count-only dossier diagnostics.
- Full host/protocol/RX-only checks passed; focused freshness, column-width
  and identity checks also pass after the final view changes. Inspected
  paired active/cached/missing/crowded/multi-link/cached-name previews.
  Preview tooling converted 85 source fixtures, including actual onboard
  views/chrome; no device screenshot claim is made.
- Final Cardputer image flashed with verified hashes to ESP32-S3 MAC
  `50:78:7d:ce:6d:64`: static RAM 63,360 bytes (19.3%), flash 761,469
  bytes (22.8%). No probe firmware or protocol literal changed.
- Live client-key-handler test passed: dossier stayed active while both
  authoritative snapshot timestamps advanced; Enter/Back, System detour/
  return and pause/restart preserved the intended inspection/receiver
  state. A second live test exercised request-tab Enter/Back on the final
  image. These inject input through the real onKeys path, not keyboard
  hardware edges; physical layout/control acceptance remains with user.
- Final diagnostics: ST7789 at 8 MHz, SD ready, zero write errors, heap
  free 179,100 bytes, minimum 173,756, largest block 159,732. Last external
  cycle 44,174 us, not a sustained frame-rate qualification. Sniffer client
  list is left receiving, with RAM debug mode confirmed off. No field
  identities or payload rows were copied into repo artifacts.

## 2026-11-17 — Automatic heap chart scale for device comparison

- User confirmed the resource monitor is visually successful and physical
  Back/`i` controls work, then requested trying an automatic vertical scale.
- Retained free heap and exact KB values. Both traces share one 60-second
  axis labelled AUTO, with a minimum 10-percentage-point span, 1-point
  padding where possible and outward-rounded 5-point bounds. Tick labels
  update to the actual range; an empty chart defaults to 0–100%.
- Preserved 0.01-percentage-point samples for plotting: the old whole-percent
  rounding could hide genuine small changes even after axis zoom. Values
  still clamp to valid 0–100%; zero totals, missing updates and restart
  gaps keep their existing semantics. No synthetic samples or interpolation
  were introduced.
- Added test-first cases for fractional changes, zoom, empty data, full-range
  and near-100% values, then a broad pairwise bounds/range check. A copied
  mutation removing the minimum span was rejected. Verification and the
  physical review build are recorded below.
- Full host/protocol/RX-only suite and focused scale tests passed. Inspected
  active/stale source previews with the new axis. Cardputer build/upload
  passed with hash verification (62,960 bytes static RAM, 749,637 flash).
- Live chart test passed: channel samples 4 -> 10 -> 16 while System was
  open, probe status sequence 11 -> 14, then 20 samples after returning to
  the monitor. ST7789 remains at 8 MHz, SD ready, zero write errors, free
  heap 184,216 bytes and largest block 159,732. System is left open with
  channel 6 receiving; RAM debug mode confirmed off. Visual preference for
  the new zoom was subsequently confirmed by the user as easier to read
  without being too jumpy. Physical System Back/`i` navigation was also
  confirmed; other cards and the P5 cold-boot/soak gates remain open.

## 2026-11-17 — Resource monitor flashed; live receive continuity passed

- User reconnected the Cardputer and authorized flashing/testing. Upload
  identified ESP32-S3 MAC `50:78:7d:ce:6d:64`; flash hashes verified. Added
  count-only `dump` fields for scope activity/sample count, System view and
  probe status sequence so continuity is measurable without field identifiers.
  Final static RAM 62,960 bytes (19.2%), flash 748,961 bytes (22.4%).
- Passive channel-6 monitor: 4 measured windows before entering System,
  then 10 and 16 while System remained open. Probe status sequence advanced
  19 -> 22 during that interval. Returning through the same card-navigation
  path retained reception and reached 20 windows. System was then reopened;
  the receiver remains running for user review. No synthetic RF data was
  injected; no LoRa configuration or capture session was started.
- End-of-test diagnostics: ST7789 initialized at 8 MHz, SD ready, zero write
  errors, free heap 184,536 bytes, minimum 182,716, largest block 159,732.
  Last observed external frame cycle 38,277 us; not a sustained frame-rate
  qualification. Debug mode was confirmed disabled in RAM afterward.
- USB exercised card navigation, not physical keyboard edges. On-device
  visual review and physical `i`/Back gestures remain user checkpoints.
  No new SD readback/soak claim is made from the idle write counters.

## 2026-11-16 — External System resource monitor

- User requested a Task Manager-inspired external System card instead of
  repeating the onboard summary, and explicitly chose to keep receivers
  running while viewing System. The onboard Info layout and external Link
  health view stay intact.
- Added a 60-second, fixed 0–100% free-heap chart: green deck, pink probe,
  exact free KB legend, and activity rows for reported PHY mode, reported
  LoRa lane, GNSS and actual Drive/LoRa capture files. This shows resource
  history and engine state, not invented CPU or per-task memory metrics.
- Deck samples at 1 Hz across all cards. Probe STATUS polling runs at most
  every two seconds after foreground/health work, never during an in-flight
  command, paging/retry/handoff or snapshot scan. A bounded 61-sample ring
  per board deduplicates probe replies; old values are not plotted as new
  measurements. Gaps, invalid totals and probe restart/link epochs break
  trace connections; stale probe traces turn slate and show status age.
- Added `i` to open System without stopping the originating receiver, with
  Back/`i` returning to that tool (including a locked scope or raw packet
  inspector). Navigating to a different receiver still stops the saved
  PHY origin through the existing acknowledgement-gated handoff. System
  snapshot/navigation logic lives in `app/deck_system_state`, with pure
  navigation policy in `app/system_navigation`.
- Test-first resource fixture initially failed to compile before the model
  existed. Host fixtures now cover cadence, duplicate replies, absent data,
  zero totals, stale expiry, ring bounds, clock wrap, reboot/link epochs
  and System origin/exit ownership. Copied stale-expiry and navigation
  regressions were rejected. Synthetic active/stale/absent previews use
  the actual renderer, including a deliberate missed-update gap.
- Full host/protocol/RX-only checks passed with `ASAN_OPTIONS=detect_leaks=0`,
  as did native TFT lock/bounds and SD mount/clock mutation checks.
- Final `cardputer-adv` build passed: static RAM 62,960 bytes (19.2%),
  flash 747,937 bytes (22.4%). USB upload/device navigation review remain
  pending: the deck is absent from both sandbox and host USB device lists.
  No physical redraw-rate or scan-continuity claim is made yet.

## 2026-11-16 — Paired-screen inspection and restored constellation context

- User review liked the Packet Monitor scope but rejected the loss of
  relationship graphics and packet depth. Kept that scope and the onboard
  System/Link layouts; promoted both health hierarchies to the companion:
  paired DECK HEAP / PROBE HEAP panels, uptime, Link/GNSS/SD and OCP health.
- Wi-Fi/BLE now use phosphor RSSI constellations with selected identity
  dossiers. Angle is a stable hash of identity, not bearing; radius is
  strength, not distance. Sniffer edges represent reported AP/client or
  SSID-request rows. Wi-Fi can reuse matching client edges from an explicitly
  cached sniffer snapshot. BLE shows manufacturer ID, tracker flag and
  strength relative to the strongest observed advertiser, without invented
  association edges. AP inspection restores beacon interval/count context.
- Enter on a LoRa packet opens a frozen, validated hex/ASCII payload on both
  displays, 64 bytes per page, semicolon/period paging and Back to the list.
  RX can continue; new arrivals cannot move the inspected bytes. Controls
  and non-ASCII render as dots. Framing is a guess; no MeshCore decoder added.
- Back from a locked Packet Monitor waits for scoped PHY STOP acknowledgement
  before restoring the cached survey and cursor. Stop/resume retains the
  locked channel instead of silently starting the broad survey. Leaving an
  inspection level no longer leaves its whole tool. Updated onboard hints.
- Test-first inspection fixture failed before implementation, then passed
  malformed hex, immutable payload, page bounds and cached-parent restore
  with real channel values/timestamps. A temporary corrupted-copy mutation
  was rejected. Full host/protocol/RX-only checks, native TFT lock/bounds
  checks and SD 8 MHz/mount checks passed. Final Cardputer build: 58,728 bytes
  static RAM, 743,633 bytes flash. Source-rendered synthetic active/empty/
  cached previews and first/last hex pages inspected; original guide compared
  in Chromium via the Playwright skill. These are not framebuffer captures.
- First upload failed during USB reset before any flash write. Retry
  identified ESP32-S3 MAC `50:78:7d:ce:6d:64`, flashed and hash-verified.
  Normal boot reports ST7789 initialized at 8 MHz, SD ready, zero write
  errors, 188,676 bytes free heap and 167,924-byte largest allocation.
  One quiet-view cycle was 58,866 us, not a sustained animation benchmark.
  Device visual/navigation qualification and a new animation-rate
  measurement remain unverified; no synthetic radio data was injected.

## 2026-11-16 — Graphical companion overhaul, ready for device review

- User authorized overhauling every external view while retaining the
  graphical 80s observatory/cyberpunk character and smooth animation,
  prioritizing Packet Monitor. The Cardputer retains navigation/actions;
  no radio command, protocol or pin assignment changed.
- Added a shared instrument vocabulary: phosphor palette, calibration grid,
  clipped corners, segmented meters, larger selected identity, and two or
  three supporting readings. Reworked Wi-Fi/BLE signal fields, observed
  AP/client and SSID-request diagrams, LoRa receive cadence, 802.15.4 LQI,
  Drive breadcrumb, GNSS fix reticle, Link/Info, AP Detail, detectors and
  Settings. Removed directional index-radar fiction, duplicated dense
  lists, persistent hex dump, competing graphs and redundant diagnostics.
  Logo/startup and normal deck controls remain intact.
- Packet Monitor now uses one measured 30-second scope of 1-second windows
  when locked, or indexed dwell-count bars during channel survey. Missing
  windows remain gaps, cached history freezes, aged data stays explicit,
  and no observation renders as a measured zero before reception. Exact
  large counts reduce type size to fit their inspector column.
- Added `external_instrument`, `external_plot`, `external_gnss_view` and
  physical 40-pixel dirty spans; adjacent spans merge and cache entries
  commit only after successful locked sends. Active frame admission is
  50 ms; quiet views 250 ms. Wi-Fi/BLE/contact snapshot copies are bounded
  to 200 ms unless focus changes. Direct reads of the fixed RGB565 strip
  buffer avoid per-pixel pixelcopy setup; initialization checks byte order.
  Static RAM is 58,504 bytes (17.9%); no full-frame framebuffer is added.
- Added real-source previews using a small host raster stub and the
  installed M5GFX font (included, not copied). Python output uses only the
  standard library. Inspected 51 populated/empty/cached synthetic states;
  fixed footer collisions, selected-peer visibility, expired-history peak
  display and a Contacts signature mismatch. Preview output is synthetic,
  not a captured device framebuffer: `/tmp/oscilla-viewport-review/`.
- Host checks passed with leak detection disabled for the ptrace sandbox.
  Added scope tests for empty data, ring order, real gaps, cached freeze,
  overflow and clock wrap; a gap-bridging mutation on a temporary header
  was rejected. Transport tests cover span bounds/locking; cache tests
  exhaust all 256 dirty-span masks plus physical row/tile ownership.
  The final adjacent-only span planner passed focused checks after the
  full host suite. Normal and animation/readback fixture builds passed.
- Hardware evidence: the new partial-span path completed 32 disposable-file
  cycles (128 KiB written/read back; cleanup complete) during a labelled
  synthetic scope. Initial animation was 6.4 FPS, with median cycle 150 ms.
  Direct buffer access raised a short SD-cycle sample to 14.6 FPS, median
  62 ms, maximum SPI span 801 us; its 4 KiB readback/cleanup passed. Merging
  across one clean tile was slightly slower (14.1 FPS, median 68 ms,
  maximum span 822 us), so that experiment was reverted. These are bounded
  synthetic measurements, not a 20 FPS or sustained-soak guarantee.
- Restored the normal review image on the confirmed ESP32-S3 deck. USB
  verified driver initialization, ongoing render frames, 8 MHz clock,
  `sd_ready=1`, zero write errors and 177,572 bytes free heap (155,636-byte
  largest block) after boot. It is not left in the synthetic bench fixture.
  Updated brand instructions/module map. Full on-device visual acceptance
  across cards, physical cold boot and sustained soak remain open.

## 2026-11-16 — ST7789 8 MHz redraw/SD readback passed

- User confirmed the corrected native-driver colours, reinserted SD, and
  reported slow updates at the conservative 1 MHz clock. Tested the same
  native transport at 8 MHz while SD stayed at 8 MHz; the user confirmed
  clean colours/output without streaks or flicker as the counter advanced.
- The existing armed coexistence fixture completed all 32 cycles:
  512 accepted 256-byte writes, 131,072 bytes written, 131,072 bytes read
  back and verified, final `phase=passed`, `cleanup=removed`. Every cycle
  removes its unique disposable file before advancing. The fixture ran
  for roughly four minutes with TFT redraw interleaved under the shared
  storage lock. This is bounded coexistence evidence, not a sustained soak.
- Set the normal ST7789 clock to 8 MHz (1 MHz remains available as a bench
  override). A full RGB565 frame's wire-time floor falls from 1.23 s to
  154 ms; the renderer's 330 ms data cadence remains a schedule, not a
  measured refresh-rate guarantee. USB diagnostics now report configured
  clock alongside initialization and row counts.
- Rebuilt and restored the normal `cardputer-adv` image to the confirmed
  ESP32-S3 deck. Post-flash USB confirmed `clock_hz=8000000`, native row
  sends increasing 584→622, `sd_ready=1`, and zero write errors. The
  normal app is restored; the device is not left in the bench fixture.
- Final full host checks passed with `ASAN_OPTIONS=detect_leaks=0`,
  including lock-bypass and overclock rejection. Normal, standalone colour
  and coexistence builds passed. `git diff --check` passed. Updated current
  architecture/pin guidance, the P5 bounded-readback gate and hardware log.
  Physical power-cycle/cold-boot qualification and sustained soak remain open.

## 2026-11-16 — Native ST7789-only transport and polarity correction

- The user reported the command-adapter normal image still produced only
  backlight, despite the standalone native-driver colour fixture working,
  and authorized retiring the old display support. The failed command
  adapter is not counted as physical integration success.
- Replaced raw SPI initialization and pixel writes with `external_tft`,
  a full M5GFX ST7789 device. It uses the SD's same SPI2 controller/global
  Arduino SPI backend, retains GPIO39 MISO for SD, initializes after storage,
  and takes the existing bounded storage lock for setup and each row.
  Native CS, RGB565/byte order and rotation replace manual command parsing.
  TFT remains write-only, 1 MHz; SD remains 8 MHz.
- Made ST7789 the default `cardputer-adv` configuration; kept the
  `cardputer-adv-st7789` alias and the standalone reference fixture. Retired
  selectable ILI9341/no-CS profiles and removed their normal-app branches,
  parking and corner reservation. Historical fixture files and bench
  records remain. The companion renderer shrank to 300 lines, retaining
  all views and physical-row caching; internal deck controls are preserved.
- Added `display_status` driver initialization/row counters. After flashing
  the native-driver image to the confirmed ESP32-S3 deck, USB reported
  initialization and row sends increasing from 592 to 625. User confirmed
  the interface was visible, but reported white background/blue logo.
  Changed `panel.invert` from true to false in the normal driver and the
  standalone reference, rebuilt and flashed the correction. Final colour
  confirmation and SD reinsertion are pending.
- Native-driver host tests cover shared-controller configuration, bounds,
  failed initialization/locking, and absence of unlocked sends; a
  temporary lock-bypass mutation is rejected. The polarity expectation
  failed against the prior true flag and passed with false. Renamed the
  storage check to `check_sd_storage.py`, retaining mount/remount and
  overclock rejection. Full host checks passed before the one-flag polarity
  correction; the focused native-driver check passed after it. Normal and
  coexistence builds passed, and the corrected normal image was flashed.
  Updated module map, current hardware/profile guidance and README.
- SD/display readback, cold boot with SD and sustained soak remain open.

## 2026-11-16 — Normal-deck ST7789 integration

- User confirmed colour output from the isolated ST7789 fixture and
  authorized normal-firmware integration and SD coexistence checks.
  Added `cardputer-adv-st7789` with the same 1 MHz TFT clock, landscape,
  RGB565, BGR and inversion settings. SD is capped at 8 MHz for this
  profile. The original no-CS and ILI9341 CS profiles remain selectable.
- Added `external_st7789_init` to consume the pinned M5GFX ST7789 command
  sequence through the existing Arduino SPI transaction; it never creates
  another bus. Storage still initializes before TFT traffic and the same
  storage lock covers transactions. CS remains high during SD access;
  CS-equipped fixtures skip no-CS window parking. Startup diagnostics now
  describe commands sent instead of claiming display acknowledgement.
- Host regression records initialization commands, delays, RGB565,
  rotation and inversion; a deliberately wrong inversion on a temporary
  source copy is rejected. Storage tests cover the ST7789 clock cap on
  mount/remount and reject an overclock mutation. Full host checks passed
  with `ASAN_OPTIONS=detect_leaks=0`; the updated storage checks passed
  separately after the cap change. Initial builds passed for normal
  ST7789, ILI9341 CS, and no-CS profiles; the final ST7789 image built and
  flashed successfully to the confirmed ESP32-S3 deck. Final regression
  rebuilds of both ILI9341 profiles also passed after the shared-bus changes.
- USB `display_status` confirmed the integrated app is running, SD absent
  as expected, zero write errors, and GNSS searching. User visual
  confirmation of the normal interface and SD reinsertion are pending.
  Added and built `sd-st7789-check`, using the existing armed disposable
  file fixture (32×4096-byte write/read/cleanup cycles) at TFT 1/SD 8 MHz.
  It has not yet run on this panel. Cold-boot/soak qualification remains
  open; the 1 MHz clock implies at least 1.23 s of wire time per full frame.
- Updated Rev D §5.4, the module map, README profile guidance and roadmap
  without declaring the replacement-panel qualification gate complete.

## 2026-11-16 — Isolated ST7789 test for replacement TFT

- The user observed a white external screen after the ILI9341 CS-profile
  flash. Goldenmorning's catalogue lists GMT028-05 as ST7789:
  <https://goldenmorninglcd.com/tft-display/>. Treat this as a controller
  candidate until the physical panel renders; the earlier app's ready
  message was not a display acknowledgement.
- Added `tft-cs-check` / `bench/tft_cs_check.cpp`: existing M5GFX ST7789
  driver, 240×320 geometry rotated to landscape, write-only at 1 MHz,
  GPIO5 CS and Rev D signal mapping. The internal LCD labels the test;
  external bars and solid red/green/blue/white frames cycle every three
  seconds. No SD driver, GNSS, or probe traffic runs in this fixture.
  Updated the module map and Rev D replacement-panel record.
- User confirmed power off, microSD removal, and USB reconnection before
  flashing. The initial compile needed an explicit ST7789 panel header;
  the corrected build passed. Flashed all boot images and app to the
  confirmed ESP32-S3 deck `50:78:7D:CE:6D:64`; flash verification passed.
  USB then reported consecutive `pattern=1` and `pattern=2` sends, proving
  the test booted and its loop runs. Physical colour output remains pending.
- `ASAN_OPTIONS=detect_leaks=0 ./tools/check_protocol.sh` passed, including
  conformance and mutation checks; the receive-only guard was rerun after
  the header correction and passed. `git diff --check` passed. The normal
  deck firmware has not yet been converted to ST7789; SD coexistence and
  cold-boot/soak qualification remain open.

## 2026-11-16 — Replacement CS TFT firmware flashed

- User reports the GMT028-05 V1.1 2.8-inch 240×320 SPI display powered at
  3.3 V with its backlight on, and CS connected to Cardputer EXT13/GPIO5.
- Built and flashed the existing `cardputer-adv-cs` profile to the confirmed
  ESP32-S3 deck, USB serial `50:78:7D:CE:6D:64`. PlatformIO wrote and verified
  the bootloader, partition table, boot application stub, and application.
  USB diagnostics then reported `deck state=ready` and `sd_ready=1`, with
  zero write errors. This confirms app boot, not physical TFT output or
  sustained SD/display coexistence; visual confirmation remains pending.
- The CS-profile build and all 27 OCP selftests passed. The first host-check
  run encountered LeakSanitizer's ptrace restriction; reran with leak
  detection disabled (`ASAN_OPTIONS=detect_leaks=0`), and the full host-check
  suite passed. No firmware source
  changes were needed for this flash.
