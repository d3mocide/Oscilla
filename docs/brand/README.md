# Oscilla Brand & UI System

**Visual/UI authority for Oscilla.** This document defines the brand, display
language, and interaction presentation. It does not override hardware
authority in Research/c5-backpack-design.md, software architecture in
DESIGN.md, or protocol literals in protocol/ocp.h.

The companion [visual guide](oscilla-master-brand-ui-guide.html) is for human
review. This Markdown file is the implementation handoff for coding agents.

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

Status-dot labels: B backpack, G GNSS, S SD, R RF activity. Preserve the dot
order so operators learn the cluster spatially.

### Deck legibility budget

- Primary interaction labels: roughly 14–16 physical px.
- Supporting values: roughly 9–10 physical px.
- Footer/context: 8–9 physical px, one short line only.
- Limit list views to four rows. Do not duplicate the view title in the body.
- The top bar, transport strip and footer consume approximately 20 px, 18 px
  and 14 px respectively; the remaining space belongs to the instrument data.

### Navigation

The target top-level deck is shallow:

1. **SYSTEM** — Link, Info
2. **OBSERVE** — Wi-Fi, BLE, 802.15.4, Sub-GHz
3. **ANALYZE** — Packet Monitor, Sniffer, captures
4. **DRIVE** — GNSS, markers, logging, session state
5. **LOGS** — offline review

Keyboard contract: comma and slash move sibling cards/rows; Enter opens or
selects; semicolon and period scroll/focus; backtick stops active work and
returns; m adds a Drive marker; s starts/stops where offered.

## External ILI9341 — landscape 320 × 240

The external panel is a **display-only data viewport**. It intentionally has
no duplicated top bar, battery, status dots, live strip, command footer,
navigation or touch affordance. The deck remains complete if it is disconnected.

### Vertical composition

Use a vertical split:

- **Upper 64%:** full-width visualization or dense context field.
- **Lower 36%:** selected-item inspector in three compact columns: identity,
  measured values, condition/annotation.

This gives the visualization enough width and makes the detail panel a clear
reading dock without imitating deck chrome.

### View contracts

| Deck counterpart | External-only companion |
|---|---|
| Packet Monitor | channel occupancy bars, baseline comparison, focused-channel RF inspector |
| Sniffer | ranked relationship table, client counts, security/freshness detail, anomaly context |
| Drive | GNSS breadcrumb and marker-event history, accuracy, movement/session context |
| Sub-GHz / BLE / 802.15.4 frames | protocol cadence or dense frame context, RSSI/SNR, selected-frame decode |
| Logs | session comparison, timeline/density overview, preview counts and explicit cached state |

The external panel may show unique context or a useful visualization, but it
may not hide a required command or create a second state machine.

## Rendering constraints

- The deck SD and external TFT share SPI: use one lock, initialize SD first,
  and serialize all TFT writes.
- The TFT is write-only: never rely on touch or readback.
- Draw static structure once; redraw changed rows, bars and values only.
- Use bounded work and failure states—never spin or block a radio/UI workflow.

## Implementation acceptance checklist

- [ ] Full Lens Core is uncropped and pink-free; glyph is used under 32 px.
- [ ] Every live observation states LIVE, an age, or CACHED.
- [ ] Deck view name is in the top bar and battery is far-right.
- [ ] Main deck text meets the legibility budget; no crowded footer.
- [ ] External view contains no replicated deck chrome or controls.
- [ ] External view uses the vertical visualization/inspector split.
- [ ] Geometry communicates an actual field, relationship or orientation.
- [ ] No UI action implies or enables transmission.
