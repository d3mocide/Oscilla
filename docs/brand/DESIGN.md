---
name: Oscilla Recon Telemetry
colors:
  surface: '#151217'
  surface-dim: '#151217'
  surface-bright: '#3b383d'
  surface-container-lowest: '#0f0d11'
  surface-container-low: '#1d1b1f'
  surface-container: '#211f23'
  surface-container-high: '#2c292e'
  surface-container-highest: '#373438'
  on-surface: '#e7e0e7'
  on-surface-variant: '#c3c9b4'
  inverse-surface: '#e7e0e7'
  inverse-on-surface: '#322f34'
  outline: '#8d9380'
  outline-variant: '#434939'
  surface-tint: '#a0d75e'
  primary: '#c3fc7e'
  on-primary: '#1f3700'
  primary-container: '#a8df65'
  on-primary-container: '#3b6200'
  inverse-primary: '#406900'
  secondary: '#cdc6b4'
  on-secondary: '#343024'
  secondary-container: '#4b4739'
  on-secondary-container: '#bbb5a3'
  tertiary: '#ffe4eb'
  on-tertiary: '#640037'
  tertiary-container: '#ffbcd2'
  on-tertiary-container: '#a41e61'
  error: '#ffb4ab'
  on-error: '#690005'
  error-container: '#93000a'
  on-error-container: '#ffdad6'
  primary-fixed: '#bbf377'
  primary-fixed-dim: '#a0d75e'
  on-primary-fixed: '#102000'
  on-primary-fixed-variant: '#2f4f00'
  secondary-fixed: '#eae2cf'
  secondary-fixed-dim: '#cdc6b4'
  on-secondary-fixed: '#1f1b10'
  on-secondary-fixed-variant: '#4b4739'
  tertiary-fixed: '#ffd9e4'
  tertiary-fixed-dim: '#ffb0cb'
  on-tertiary-fixed: '#3e0020'
  on-tertiary-fixed-variant: '#8d0150'
  background: '#151217'
  on-background: '#e7e0e7'
  surface-variant: '#373438'
  void-ink: '#0B090D'
  panel-dark: '#151018'
  paper-phosphor: '#F4ECD9'
  field-green: '#A8DF65'
  dim-green: '#51763D'
  signal-pink: '#F05D9D'
  calibration-yellow: '#F5D658'
  fault-red: '#EF514C'
  muted-slate: '#AAA0A5'
  line-border: '#4B3C49'
  track-dark: '#263528'
  screen-ground: '#090B09'
  selection-glow: '#241F0D'
typography:
  headline-lg:
    fontFamily: Space Grotesk
    fontSize: 48px
    fontWeight: '700'
    lineHeight: 52px
    letterSpacing: -0.05em
  headline-lg-mobile:
    fontFamily: Space Grotesk
    fontSize: 28px
    fontWeight: '700'
    lineHeight: 32px
    letterSpacing: -0.04em
  headline-md:
    fontFamily: Space Grotesk
    fontSize: 24px
    fontWeight: '700'
    lineHeight: 28px
    letterSpacing: -0.03em
  headline-sm:
    fontFamily: Space Grotesk
    fontSize: 18px
    fontWeight: '600'
    lineHeight: 22px
    letterSpacing: -0.02em
  body-lg:
    fontFamily: JetBrains Mono
    fontSize: 15px
    fontWeight: '400'
    lineHeight: 22px
  body-md:
    fontFamily: JetBrains Mono
    fontSize: 13px
    fontWeight: '400'
    lineHeight: 18px
  body-sm:
    fontFamily: JetBrains Mono
    fontSize: 11px
    fontWeight: '400'
    lineHeight: 15px
  label-lg:
    fontFamily: JetBrains Mono
    fontSize: 12px
    fontWeight: '700'
    lineHeight: 16px
    letterSpacing: 0.08em
  label-md:
    fontFamily: JetBrains Mono
    fontSize: 10px
    fontWeight: '700'
    lineHeight: 13px
    letterSpacing: 0.10em
  label-sm:
    fontFamily: JetBrains Mono
    fontSize: 9px
    fontWeight: '700'
    lineHeight: 11px
    letterSpacing: 0.12em
spacing:
  gutter: 1rem
  gutter-mobile: 0.5rem
  margin: 1.5rem
  margin-mobile: 0.75rem
  space-xs: 0.25rem
  space-sm: 0.5rem
  space-md: 1rem
  space-lg: 1.5rem
  space-xl: 2rem
---

## Brand & Style

The design system establishes a high-discipline, military-grade terminal recon aesthetic. It bridges Cold War CRT reconnaissance telemetry, esoteric radio astronomy, and ultra-compact embedded computing. Designed explicitly for extreme contrast and information density under tactical conditions, it treats geometry strictly as an instrument of spatial relationship, never as decorative wallpaper.

### Emotional Response & Ethos
- **Dispassionate Telemetry:** The interface communicates truth, stability, and high-frequency precision. Every pixel is an absolute coordinate; every visual element is functional data.
- **Instrumental Restraint:** "Signal before spectacle." The system bans soft gradients, faux CRT scanlines, lens blooms, drop blurs, and skeuomorphic aging. The terminal is a crisp, direct lens into raw RF activity.
- **Hierarchical Priority:** Truth (raw signal state) → Action (control primitives) → Context (channel/frequency metadata) → Atmosphere (deep void frame).

### Visual Metaphor
The UI acts as an electronic spectrum deck and vector scope. Structural rules and dimensional bounding boxes mimic vector reticles, circuit boundaries, and frequency grids, grounding high-contrast neon data readouts directly against an opaque, low-reflectance void ground.

## Colors

The color palette is built for solid 16-bit rendering pipelines (ESP32/TFT LCDs) and rigorous high-contrast visibility. Semantics are immutable across views:

- **Primary (`#A8DF65` - Field Green):** Represents nominal operations, passive readiness, confirmed RF signal lines, bounding reticles, and system locks.
- **Secondary (`#F4ECD9` - Paper Phosphor):** The core reading lens. Used for active alphanumeric values, telemetry readout streams, labels, and vector lens lines.
- **Tertiary (`#F05D9D` - Signal Pink):** Used exclusively for active focus selections, transient wireless catches, tracker detection warnings, and hot threshold crossing.
- **Neutral (`#0B090D` - Void Ink):** Deepest ground state. Absorbs visual clutter and maximizes the perceived brightness of phosphor readouts.

### Semantic & Operational Colors
- **`calibration-yellow` (`#F5D658`):** Target cursors, manual threshold bounds, active selection pills, and telemetry metadata tags.
- **`fault-red` (`#EF514C`):** Strict fault reservation. Only triggered during bus contention, lock timeouts, battery depletion warnings, and transport frame errors.
- **`muted-slate` (`#AAA0A5`):** Subdued headers, inactive node addresses, timestamps, and peripheral chrome.
- **`line-border` (`#4B3C49`) / `dim-green` (`#51763D`):** Internal structural rules and cell separators. `dim-green` designates telemetry and instrument divisions; `line-border` denotes physical deck card containers.
- **`panel-dark` (`#151018`):** Card container fills providing 1-step depth separation from the absolute void.

## Typography

Typography functions as an analytical instrument. Columns must remain mechanically rigid to prevent visual drift during real-time data streaming.

### Font Roles
- **Headlines & Instrument Titles (`Space Grotesk`):** Highly disciplined, geometric sans with deliberate mechanical eccentricities. Set tight (`-0.03em` to `-0.05em`) to emphasize mass and authority in titles, high-level mode switches, and primary frame markers.
- **Telemetry & Body Data (`JetBrains Mono`):** Fixed-width, pixel-aligned monospace for all telemetry lines, payload dumps, register addresses, table columns, and keyhints.
- **Labels & System Badges (`JetBrains Mono`):** Compact uppercase text with wide tracking (`0.08em` to `0.12em`) ensuring clear legibility across cramped embedded panels and high-density status ribbons.

### Micro-Viewport Disciplines
For embedded viewports (Cardputer 240×135 and ILI9341 320×240), use `label-sm` (9px) and `label-md` (10px) to conserve vertical line budget. Maintain single-line vertical alignment without wrapping; truncate alphanumeric overflows with terminal carets (`…` or `>`).

## Layout & Spacing

The layout model is governed by structural enclosures and segmented telemetry strips rather than generic open-ended whitespace.

### Layout Philosophy & Frameworks
- **Primary Embedded Deck (Cardputer ADV - 240×135):** Strict fixed vertical allocation:
  - Header Deck Bar: `20px` height (View name + system indicators `B/G/S/R` + battery glyph).
  - Transport Strip: `18px` height (Live state telemetry `● LIVE` / `◇ CACHED` + channel hop status).
  - Data Viewport: `83px` height (Cap of 4 list rows or 2-column key-value matrix).
  - Command Foot: `14px` height (Keyhints: `ESC:STOP`, `TAB:MODE`, `ENT:AP DETAIL`).
- **External Viewport (ILI9341 - 320×240 Landscape):** 2-pane telemetry viewport with a 64% / 36% horizontal split:
  - Primary Packet Monitor Grid (64% width): Waterfall, signal reticle, and live packet stream.
  - Secondary Inspector (36% width): Selected node parameters, BSSID, RSSI/SNR histogram, and auth flags.
- **Desktop / Web Monitor View:** Standard 12-column responsive layout conforming to a `1240px` maximum bound, dropping to 2-column or single-column decks on compact screens.

### Spatial Rhythm
- `space-xs` (4px): Micro gaps between indicator dots, cell padding, and status tags.
- `space-sm` (8px): Gaps between telemetry items, row dividers, and control buttons.
- `space-md` (16px): Standard internal padding for cards, frame perimeters, and modular blocks.
- `space-lg` (24px): Boundary margins for primary view ports.
- `space-xl` (32px): Major deck section divisions.

## Elevation & Depth

Visual hierarchy is executed entirely through planar opacity, opaque color layering, and crisp 1px structural borders. Diffuse dropshadows, glassmorphic blurs, and translucent soft layers are forbidden.

### Depth Archetype: Bold Tactical Borders & Tonal Planes
- **Ground Tier (Level 0 - Absolute Void):** `#0B090D` (Void Ink). The master background for canvas viewports, raw command lines, and embedded screen chassis.
- **Container Tier (Level 1 - Deck Surface):** `#151018` (Panel Dark). Opaque fills for data cards, inspectors, and diagnostic modules. Defined by a continuous `1px solid #4B3C49` outer perimeter.
- **Active Reticle Tier (Level 2 - Focused Lens):** `#090B09` (Phosphor Ground). Used exclusively inside data visualization areas, RF waterfall charts, and memory dumps. Framed by `1px solid #51763D`.
- **Physical Hard Drop (Hardware Simulation Only):** When representing physical instrument decks on modern desktop displays, use a single brutalist offset projection: `box-shadow: 8px 9px 0px #180E15`. Never apply a blur radius.

## Shapes

The geometric framework is uncompromisingly sharp (`roundedness: 0`). Curved edges undermine structural alignment on low-resolution embedded displays and conflict with the vector scope motif.

### Shape Geometry Rules
- **Panels, Buttons, Tags, and Cards:** Corner radius is strictly `0px`.
- **Status Dots & Radians:** Circular form factors (`50%` radius) are reserved solely for live radio transmission dots (`.dot`), GNSS satellite lock markers, and target pins on polar maps.
- **Reticles & Framing:** Hexagonal boundaries, 45-degree corner notches (clip-paths), and axial crosshairs are preferred over rounded rectangles to construct an authentic military recon instrument aesthetic.

## Components

### Buttons & Key Triggers
- **Base Style:** Solid rectangular boxes, `roundedness: 0`, 1px solid border (`#4B3C49`), monospace uppercase typography.
- **Default State:** Background `#151018`, text `#F4ECD9`.
- **Active / Focused:** Background `#F5D658` (Calibration Yellow), text `#0B090D` (Void Ink), border-color `#F5D658`.
- **Alert / Destructive:** Background `#F05D9D` (Signal Pink), text `#0B090D`.

### Status Indicators & Indicators (`.dot`)
- **Dimensions:** Rigid 6px to 8px circular points (`border-radius: 50%`).
- **Nominal Ready:** Solid `#A8DF65` (Field Green).
- **Target Flagged / Tracking:** Solid `#F05D9D` (Signal Pink).
- **Awaiting Lock / Threshold:** Solid `#F5D658` (Calibration Yellow).
- **Hardware Fault / Bus Contention:** Solid `#EF514C` (Fault Red).

### Telemetry Badges & Tags
- **Structure:** Padding `2px 6px`, `0px` radius, 1px solid border.
- **Normal Protocol Tag:** Border `#51763D`, text `#A8DF65`, background `#090B09`.
- **Active Filter:** Border `#F5D658`, text `#0B090D`, background `#F5D658`.
- **Alert Flag (AirTag / Deauth):** Border `#F05D9D`, text `#F05D9D`, background `#151018`.

### Data Tables & Scan Lists
- **Rows:** Maximum 4 rows displayed concurrently on 135px screens; expanded tabular format on 240px external display.
- **Dividers:** `1px solid #263528` separating individual observations.
- **Selected Row:** High-contrast inverse highlight with background `#241F0D` and a `2px` left border in `#F5D658`. Text transitions to `#F4ECD9` with a leading `›` caret.

### Cards & Deck Modules
- **Body:** Background `#151018`, border `1px solid #4B3C49`.
- **Header Ribbon:** Segmented with a bottom border `1px solid #51763D`, displaying category and index in uppercase `JetBrains Mono` bold.
- **No Outer Gaps:** When placed side-by-side in grid arrays, cards share mutual 1px borders to eliminate wasted screen space.

### Signal Meters & Bar Gauges
- **Track:** Background `#263528`, height `4px` to `8px`, `0px` border-radius.
- **Nominal Fill:** Solid `#A8DF65` (Field Green).
- **Peak / Hot Fill (RSSI > -60dBm):** Solid `#F05D9D` (Signal Pink).
- **Subdivided Segments:** Rendered as discrete 2px-wide block ticks separated by 1px blank voids to mimic vintage LED segment arrays.
