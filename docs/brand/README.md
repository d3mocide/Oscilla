# Oscilla Brand & UI System

**Visual/UI authority for Oscilla.** This document defines the brand, display
language, and interaction presentation. It does not override hardware
authority in Research/c5-backpack-design.md, software architecture in
DESIGN.md, or protocol literals in protocol/ocp.h.

The companion [visual guide &amp; UI lab](../deck-ui-state.html) is the single HTML
source of truth for human review (synced from compiled C++ renders via
`tools/render_external_views.py --sync-guide-assets`). This Markdown file is
the implementation handoff for coding agents.

## Non-negotiables

- **Hero line:** OBSERVE THE NOISE.
- **Mark:** Lens Core only. Do not revive discarded logo concepts.
- **Receive-only:** UI language remains observe, survey, listen, inspect and
  anomaly-monitoring. Do not introduce transmit-shaped controls.
- **Truth first:** live, aged and cached data must be visibly distinct.
- **Deck owns control:** the Cardputer controls navigation, actions and system
  state. The external display is never required to operate the instrument.

## Lens Core

| Asset | Use |
|---|---|
| [full mark](assets/lens-core.svg) | Hero, enclosure, project page, and any placement at 32 px or larger |
| [small glyph](assets/lens-core-glyph.svg) | Boot renderer or UI placements below 32 px |

The mark is a green hexagonal field with orthogonal orbits, a cream lens, and a
yellow focal dot. **Signal Pink is not part of the logo.**

- Keep at least one outer-orbit stroke width of clear space on every side.
- Never crop the hex or the orbital paths.
- Do not add glow, gradients, scanlines, texture, or an outline to the mark.

## Palette

| Token | Hex | Meaning |
|---|---:|---|
| Void Ink | #0B090D | screen ground |
| Paper Phosphor | #F4ECD9 | primary text and lens |
| Field Green | #A8DF65 | ready, frame, grid, dimensional field |
| Signal Pink | #F05D9D | selected item or notable detection |
| Calibration Yellow | #F5D658 | cursor, threshold, focused reading |
| Fault Red | #EF514C | faults only |

Use solid 16-bit color. No alpha, gradients, bloom, or hairline-only
information. Color augments text; it never supplies the only meaning.

## Type

- **Brand/display:** Chakra Petch Bold, uppercase.
- **Device/data:** IBM Plex Mono Bold, or the closest crisp fixed-width M5GFX
  font available.
- Prefer high contrast and stable columns over clever typography.

## Design philosophy

Oscilla is a field instrument by way of a **retro sci-fi mystic observatory**:
CRT-era technical diagrams, ASCII structure, luminous phosphor colors, sacred
geometry and an 80s vision of invisible fields.

Geometry is functional, not wallpaper. Hexes, orbits, axes and grids should
orient a reading, show a relationship, or frame a mode. They must never sit
behind essential text or make data harder to parse.

The hierarchy is:

1. Truth
2. Action
3. Context
4. Atmosphere

Use empty Void Ink deliberately. It creates visual quiet around a selected
signal, anomaly or focal point. Avoid fake damage, scanline effects, noise
textures and desktop-style glass effects; the result should feel native to the
hardware.

## Cardputer ADV — 240 × 135

### Shared chrome

The deck is the sole control surface.

| Position | Required content |
|---|---|
| Upper left | Card/view name such as OBSERVE / SWEEP; never OSC |
| Upper right | Backpack, GNSS, SD and RF status dots in that order |
| Far right | Battery icon, always terminal/rightmost |
| Transport strip | LIVE or CACHED, source freshness, and backpack ping/pong |
| Footer | One concise keyboard hint line |

Status dots are unlabelled (user direction, 2026-10-09). Preserve their order:
backpack, GNSS, SD, RF activity, followed by the battery.

### Deck legibility budget

- Primary interaction labels: roughly 14–16 physical px.
- Supporting values: roughly 9–10 physical px.
- Footer/context: 8–9 physical px, one short line only.
- Scrollable deck list views use a 12 px row pitch (`kListTop = kBodyTop + 14`, `kListRowHeight = 12`) with the Link-style yellow left rail (`drawRowHighlight`) and ASCII separators only (`/` — never UTF-8 middots in CP437 `glcdfont`). Show **6 rows** when no lower status line is needed (Wi-Fi Scan, BLE Scan without tracker alerts, Deauth Detect, Offline Sessions), or **5 rows** when a genuinely non-redundant status/ticker line (`LAST`, `EXT`, `FOLLOW?`, or LoRa/Packet Monitor framed bands) earns `kDetailRowY`. Do not duplicate the view title in the body.
- The top bar, transport strip and footer consume approximately 20 px, 18 px
  and 14 px respectively; the remaining space belongs to the instrument data.

### Navigation

The target top-level deck is shallow:

1. **SYSTEM** — Link, Info
2. **OBSERVE** — Wi-Fi, BLE, 802.15.4, LoRa
3. **ANALYZE** — Packet Monitor, Sniffer, captures
4. **DRIVE** — GNSS, markers, logging, session state
5. **LOGS** — offline review

Keyboard contract: comma and slash move sibling cards/rows; Enter opens or
selects; semicolon and period scroll/focus; backtick returns one inspection level, stopping only when leaving a receiver; m adds a Drive marker; s starts/stops where offered.

## External ST7789 — landscape 320 × 240

The external panel is a **display-only data viewport**. It intentionally has
no duplicated top bar, battery, status dots, live strip, command footer,
navigation or touch affordance. The deck remains complete if it is disconnected.

### Vertical composition

Use a vertical split:

- **Upper 64%:** one dominant measured graphic: scope, signal field,
  relationship diagram, quality meter or breadcrumb.
- **Lower 36%:** selected identity in larger type, then two or three readings.
  Prefer empty space over mandatory columns or repeated metadata.

This gives the visualization enough width and makes the detail panel a clear
reading dock without imitating deck chrome.

### View contracts

| Deck counterpart | External-only companion |
|---|---|
| Packet Monitor | One scrolling 30-second trace of measured 1-second windows when locked; indexed dwell-count bars during channel survey. Missing windows remain gaps; cached traces freeze. |
| Wi-Fi / BLE | RSSI constellation with a selected AP/device dossier. Identity fixes angle; radius represents measured strength. Wi-Fi may show observed AP/client edges from a labelled cached sniffer snapshot; BLE never invents associations. |
| Sniffer | AP/request-centred RSSI cluster; selected edge stays prominent. Enter opens a client-centred dossier of reported AP or SSID-request links, while reception continues. Composite identity survives reordering; missing rows retain explicitly marked last observations. |
| Drive / GNSS | North-up RAM breadcrumb or fix-quality reticle, fix freshness, saved AP count and recording/closing state. Accumulated path is explicitly RAW GNSS PATH, including position noise; it is not a filtered odometer. No invented satellite positions. |
| LoRa RX | Received-frame cadence and focused RSSI/SNR/length. Enter freezes the selected packet on both displays for paged hex and safe ASCII; Back returns to the packet list without stopping RX. Framing stays a guess. |
| 802.15.4 | Measured LQI instrument and selected MAC inventory identity; no inferred topology or application-protocol identity. |
| Deauth / correlation | Recent event counts by observed channel, or actual movement-correlation steps. An alert is not proof of following. |
| Link / Info / Settings | Link/GNSS/SD health hierarchy; System resource monitor with 60-second deck/probe free-heap traces and reported receiver/logging activity; deck brightness meter. The external backlight remains fixed on. |

### Graphical review direction — 2026-11-16

Keep the 80s observatory/cyberpunk feel through phosphor colours, clipped
instrument corners, calibration ticks, segmented meters and focused traces.
Use one graphic and a quiet inspector. Avoid dense tile belts, competing
plots and tiny repeated labels. Preserve inspection depth through explicit drill-downs. Radar rings encode RSSI, with stable identity angles and a no-bearing label; only observed relationships create edges.
The Lens Core logo remains unchanged on startup. No duplicated controls or
keyboard footer belong on the companion.

Animate elapsed receive time and real observation changes, with coherent
strip snapshots. Numerical readings remain exact; never invent intermediate
RF samples or bridge missing measurement windows. Active views target a
50 ms frame admission cadence and quiet views 250 ms; these are targets,
not proof of achieved frame rate. Actual redraw cost depends on changed
pixels and shared-bus work and must be checked on the panel.

Packet Monitor keeps its scope. Back from a locked channel stops that PHY
stream, waits for its acknowledgement, then restores the cached channel list
and cursor. `s` resumes the current mode, including the same locked channel.
Sniffer selection follows client/AP or client/SSID identity across snapshot
replacement. Enter opens the paired dossier; Back returns to the same row
when still present, otherwise a bounded list position. The dossier continues
to refresh and is marked ROW MISSING when its key disappears, retaining its
last observed values and timestamp. `s` pauses/restarts reception without
closing the dossier; `x` switches Clients/Requests from the list. The deck
keeps the LAST ticker, full client MAC and fixed channel/band/RSSI columns.
Clusters/dossiers draw at most 12 links, always include the selection and
show drawn/total counts. Root radii encode RSSI measured at the probe;
dossier orbital placement encodes identity only. Cached AP scan names are
labelled SCAN NAME; a requested SSID never implies an AP association.

System is an inspection destination: `i` opens it from a tool without stopping
its receiver; Back or `i` returns. The onboard System summary stays intact.
External heap history collects across all cards: deck at 1 Hz, probe on unique
background status replies (normally 2 seconds). Green is deck; pink is probe.
Both traces share an automatic free-heap percentage axis, labelled AUTO,
with at least a 10-percentage-point span and rounded 5-point boundaries.
Fractional samples retain 0.01-point precision; exact KB values remain in
the legend. Missing samples and probe restarts create gaps, stale probe data
turns slate, and the task panel marks aged status. PHY mode and LoRa state
come from reported lane ownership/state, supplemented by the deck's known
mode; GNSS and capture logging remain separate. No fabricated CPU load or
per-task memory is implied.

Mesh/LoRa packet inspection copies the selected payload; incoming packets
cannot shift its bytes or page. Both displays use the same 64-byte page.

The external panel may show unique context or a useful visualization, but it
may not hide a required command or create a second state machine.

### Observatory v2 visual contract — 2026-11-20

This section is the binding implementation direction for external 320 × 240
companion views (and the shared typography/surface rules for future deck
polish). The goal is a **techno-futuristic mystic observatory with real lab
discipline** — a sci-fi field instrument brought into real life without ever
fabricating telemetry.

- **Shared chrome only (`observatory_chrome.h`):** Use `obsFrame()` (9×9 mode
  sigil, uppercase Chakra Petch header, chamfered truth-state pill, and
  chamfered octagonal aperture with bright `kFieldGreen` corner accents and
  center datum ticks), `obsDock()` (solid `kPanelDark` `#151018` lower reading
  dock `y=156..239` with `kDimGreen` top rule, 42 px datum bar, `y=198`
  sub-rule, and `kLineBorder` vertical metric dividers), `obsCard()` (chamfered
  `kPanelDark` inset dossier cards), and `obsBay()` (auto-fitting dock bays).
  Do not hand-draw ad-hoc frames or fall back to flat bare lines on `#0B090D`.
- **Typography stack (`observatory_fonts.h`):** Use the 1-bit flash bitmap
  fonts generated by `tools/gen_observatory_fonts.py`:
  - `FontId::Head` (Chakra Petch Bold 12 px) — top-bar mode titles.
  - `FontId::Title` (Chakra Petch Bold 16 px) — Reading Dock primary identity
    and dossier callouts.
  - `FontId::Hero` (Chakra Petch Bold 20 px) — Reading Dock primary readings
    and big focal figures.
  - `FontId::Mono` (IBM Plex Mono Medium 11 px) — MACs, coordinates, security
    strings, and secondary telemetry.
  - Built-in 5×7 `glcdfont` at **`1×` only** (`obsMicro()`) — axis ticks,
    bay labels, badges, and caveats. **Never use integer-scaled `2×` or `3×`
    `glcdfont`** in new or upgraded views.
- **Surface & focal color discipline (strictly `theme.h` solid RGB565):**
  - `kVoidInk` (`#0B090D`) for the upper visualization aperture.
  - `kPanelDark` (`#151018`) for the lower Reading Dock and inset dossier cards.
  - `kTrackDark` (`#263528`) for dotted/crosshair grids, oscilloscope drop-stems,
    empty bar segments, and `LIVE`/`FIX` pill fills.
  - `kSelectionGlow` (`#241F0D`) for `WARN`/`FROZEN` pills and focused pulse
    column bands.
  - `kCalibrationYellow` (`#F5D658`) for the primary measured reading and active
    cursor/node core.
  - `kSignalPink` (`#F05D9D`) strictly for focal punctuation: target-lock rings
    and leader lines, peak-hold markers, selected LoRa pulse crowns, north-up
    arrowheads, and fix-lost/anomaly indicators.
  - Zero alpha, zero gradients, zero bloom, zero out-of-palette colors.
- **Sacred geometry as truthful calibration:**
  - Inscribed Lens Core hexagons (`obsHexagon`) and dashed/ticked astrolabe
    reticles orient real scales — RSSI strength rings (`obsRadar`, with `-40`,
    `-65`, `-95` anchored directly on their true inner-to-outer rings, never a
    detached side column) and GNSS HDOP precision tiers (`<1.5`, `<3.0`, `<6.0`,
    never invented satellites).
  - LoRa cadence stems encode real RSSI height and byte-length cap width,
    paired with calibrated RSSI and zero-centered SNR margin meters.
  - Drive breadcrumbs expose a real dynamic scale bar from trail meters/pixel,
    a north-up compass rose, Start `[S]` vs. Live Head target lock, and real
    UTC/altitude from `GnssFix`.
  - Packet Scope uses solid vertical drop-stems and a dashed peak-hold line
    while leaving missing measurement windows visibly unbridged.
- **Visual proof required:** Render every UI change with
  `tools/render_external_views.py` and visually inspect `active`, `cached`, and
  `empty` PNGs for text collisions, divider overlaps, and leaked key hints
  before declaring a view complete.

## Rendering constraints

- The deck SD and external TFT share SPI: use one lock, initialize SD first,
  and serialize all TFT writes.
- The TFT is write-only: never rely on touch or readback.
- Keep static structure unchanged; transmit changed 40-pixel row spans only,
  merging adjacent spans. Retain physical row/tile ownership in the diff cache.
- Use bounded work and failure states—never spin or block a radio/UI workflow.

## Implementation acceptance checklist

- [ ] Full Lens Core is uncropped and pink-free; glyph is used under 32 px.
- [ ] Every live observation states LIVE, an age, or CACHED.
- [ ] Deck view name is in the top bar and battery is far-right.
- [ ] Main deck text meets the legibility budget; no crowded footer.
- [ ] External view contains no replicated deck chrome or controls.
- [ ] External view uses the vertical visualization/inspector split.
- [ ] External view uses Observatory v2 chrome (`observatory_chrome.h`) and 1-bit flash fonts (`observatory_fonts.h`), with no `2×`/`3×` `glcdfont`.
- [ ] Geometry communicates an actual field, relationship or orientation.
- [ ] No UI action implies or enables transmission.
- [ ] Active, cached, and empty states visually verified via `tools/render_external_views.py`.

### AP and advertiser dossiers

Enter opens the selected Wi-Fi AP or BLE advertiser without interrupting
continuous reception. Back returns to the same identity when present. `i`
opens System and Back restores the dossier with its receiver intact.
Wi-Fi `x` switches radio/client-cache pages; `;` and `.` select a cached
client. `r` requests a bounded passive survey, then a further `r` can capture
beacons for an AP still present in that survey. Client links come only from
the explicitly aged Sniffer cache; opening the dossier never starts Sniffer.
BLE `r` requests a bounded passive rescan. Its 16-point, 120-second RSSI
history adds only accepted observations of the pinned MAC. Continuous
discovery emits first sightings, so redraws and other devices add no points.
Cached plots freeze at their last sample and gaps stay disconnected. Company
identifiers remain raw identifiers; no vendor or physical bearing is inferred.

### Saved-session review

`o` opens LOGS from a live tool and Back returns without stopping reception.
The LOGS home card also follows Drive in the route order. `x` filters
All/Wardrive/LoRa and `r` refreshes the SD inventory. Enter drills through
session summary, paged records and saved LoRa raw hex; Back reverses one
level. Semicolon/period cross six-record pages or move within raw hex.
The external summary plots the last 32 valid record RSSI values by observation
order and labels the observed time span. It does not imply a sample cadence
or travel distance. File snapshots remain frozen while live recording grows.
Sidecar availability is shown; KML track and sidecar content review is deferred.
