# Hardware revision worklog

PCB, electrical, mechanical, enclosure, and carrier revision history. Newest
entries first. Real-hardware measurements and qualification remain in their
respective bench records under this directory; this log records design changes
and their validation status. See [Rev D](../../Research/c5-backpack-design.md)
for electrical authority.

## 2026-10-09 — Delivered no-CS ILI9341 bench variant

Rev D §5.3 now identifies the actual eight-pin panel and separate user-verified
3.3 V regulator, preserving GPIO assignments and common ground. TFT MISO remains
unconnected. The normal firmware holds reset during SD startup and caps shared
SD traffic at 8 MHz; external writes use 4 MHz and park one corner pixel.
The repeated 128 KiB write/read/delete fixture passed and normal passive LoRa
packet plus health file readback matched. User confirmed both screens readable.
This documents empirical bench coexistence, not CS isolation or completed
cold-power/current/long-soak qualification. See software WORKLOG for counters.

## 2026-09-24 — Recheck current V1.1 resistor placement

**Phase:** Hardware tooling · **By:** Codex

Re-ran the current V1.1 ERC and PCB DRC after the operator's latest resistor
placement adjustment, and regenerated the assembly preview at
`hardware/c5-wio-carrier/v1.1/renderings/assembly-current-review.png`. Refreshed
the canonical `erc-v1.1.json` and `drc-v1.1.json` reports. The schematic and
component values/nets remain unchanged.

**Verified:** KiCad 10.0.6 ERC reports 0 violations. DRC with schematic parity
reports 0 violations, 0 unconnected items, and 0 parity issues. The one
documented `footprint_filters_mismatch` metadata check remains excluded. These
are CAD checks only; physical fit, power margin, and bring-up remain open.

## 2026-09-24 — Reroute moved V1.1 pull resistors

**Phase:** Hardware tooling · **By:** Codex + operator

Kept the operator's new R1–R3 placements and rerouted their NSS, RESET,
RF_SW, +3V3, and GND connections around the 0805 pads. The previous +3V3
trunk's final segment was detoured to clear the moved signal pads. Refreshed the
assembly render and front/back soldermask previews; no schematic nets or
resistor values changed.

**Verified:** The saved pre-reroute board reported 10 DRC violations and 6
unconnected items, with 0 schematic-parity issues. Final KiCad 10.0.6 DRC
reports 0 violations, 0 unconnected items, and 0 parity issues
(`hardware/c5-wio-carrier/v1.1/verification/drc-v1.1.json`). This is CAD
verification, not physical qualification.

## 2026-09-24 — Enlarge C1 for hand assembly

**Phase:** Hardware tooling · **By:** Codex + operator

Changed C1 from 0603 to the standard KiCad `Capacitor_SMD:C_0805_2012Metric`
footprint, retaining its 100 nF value and nets. Updated the schematic footprint
field and BOM to Samsung CL21B104KBCNNNC (100 nF, X7R, 50 V, ±10%). Shifted C1
1.5 mm toward the board bottom to clear the nearby ground trace, moved its
routed endpoints with the new pad centers, and refreshed the 3D assembly and
front/back soldermask previews. No electrical values or nets changed.

**Verified:** KiCad 10.0.6 ERC reports 0 violations. DRC with schematic parity
reports 0 violations, 0 unconnected items, and 0 parity issues. These checks
confirm CAD connectivity and clearances, not hand-assembly or physical fit.

## 2026-09-24 — Enlarge V1.1 resistor packages for hand assembly

**Phase:** Hardware tooling · **By:** Codex + operator

Changed R1–R3 from 0603 to the standard KiCad `Resistor_SMD:R_0805_2012Metric`
footprint, keeping the existing 10 kΩ values, net assignments, and component
centers. Updated the schematic footprint fields and BOM to Yageo
RC0805FR-0710KL (10 kΩ, 1%, 0.125 W). Updated the 3D assembly preview. No
electrical values or nets changed. Regenerated the front/back soldermask
review plots because the larger pads also enlarge the automatic mask openings.

**Verified:** KiCad 10.0.6 ERC reports 0 violations. DRC with schematic parity
reports 0 violations, 0 unconnected items, and 0 parity issues. These checks
confirm CAD connectivity and clearances, not hand-assembly or physical fit.

## 2026-09-24 — Refresh copper fill and soldermask review after J1 move

**Phase:** Hardware tooling · **By:** Codex

Re-ran zone refill and DRC on the moved Grove footprint and adjusted fanouts,
then regenerated front- and back-soldermask SVG/PNG review plots. The thru-hole
connector and module pads retain their `*.Mask` layer openings; there are no
manually drawn soldermask polygons to relocate. This refresh changes no net or
pad mask-expansion settings. Plots are review images only, not fabrication
exports.

**Verified:** KiCad 10.0.6 DRC with zone refill and schematic parity reports 0
violations, 0 unconnected items, and 0 parity issues. Mask plots are saved in
`hardware/c5-wio-carrier/v1.1/renderings/masks/`.

## 2026-09-24 — Raise Grove connector and H2 to improve top-edge fit

**Phase:** Hardware tooling · **By:** Codex + operator

Moved J1 (Grove) 1.5 mm toward the board's top edge, from `(106, 109.5)` to
`(106, 108.0)`, leaving 1.5 mm between its F.Fab body outline and the board
edge. Moved H2 0.5 mm in the same direction, to `(105.9, 116.3)`. This
increases their relative separation because J1 moves farther; the two
courtyards now have about 0.6 mm clearance. Reworked the three short Grove
signal fanouts without changing nets, then refilled copper zones. Moved the
standalone J1 silkscreen label clear of the connector outline. The pad grid and
footprint model are unchanged.

**Verified:** KiCad 10.0.6 DRC with zone refill and schematic parity reports 0
violations, 0 unconnected items, and 0 parity issues. The rendered assembly
shows the Grove body 1.5 mm from the top Edge.Cuts. This confirms CAD geometry
only; cable insertion, printed-case clearance, and real connector fit still
need physical validation.

## 2026-09-24 — Add source-based Grove footprint layers and verify C5 socket grid

**Phase:** Hardware tooling · **By:** Codex

Added an F.Fab body outline and F.CrtYd to the V1.1 project-local
`GROVE-HW4-2.0-90D` footprint, adapted from the user's cloned Seeed OPL
footprint. The four plated-hole centers and pad dimensions match that Seeed
library entry within 0.002 mm. Kept electrical net assignments, pad numbering,
placement, and the 3D model unchanged. Enabled the `missing_courtyard` DRC
rule. Reviewed the socketed C5 carrier footprint against Seeed's C5 DIP
footprint: the 14-hole grid is the same 2.54 mm pitch / 15.24 mm row-spacing
pattern rotated into the carrier orientation, and the 17.8 × 20.975 mm body
outline is translated from Seeed's published outline. Its 1.0 mm socket drill
remains a prototype choice, not a manufacturer-mandated drill.

**Found:** Fresh KiCad 10.0.6 DRC now flags one courtyard overlap between J1
and H2; there are 0 unconnected items and 0 schematic-parity issues. The
nominal edge-to-edge gap from the 10 × 9.8 mm Grove body outline to the 2.2 mm
mounting hole is approximately 1.2 mm, but the courtyard spacing remains a
mechanical review item. No mounting hole was moved and no overlap was
suppressed. M5's published Cardputer-ADV docs identify a HY2.0-4P port and
cable pin order, but do not provide the installed connector's exact land
pattern. This footprint provides the matching Grove cable interface, not an
exact Cardputer-PCB clone. See
`hardware/c5-wio-carrier/v1.1/verification/README.md`.

## 2026-09-24 — Audit V1.1 DRC rules, refresh verification, and complete BOM

**Phase:** Hardware tooling · **By:** Codex

Reviewed all five DRC rule categories that had been ignored. Re-enabled
`footprint_type_mismatch`, `track_not_centered_on_via`, and
`tuning_profile_track_geometries`; none produced findings. Kept
`missing_courtyard` excluded because the custom Seeed Grove connector envelope
has not been established from a manufacturer drawing. Kept
`footprint_filters_mismatch` excluded because J1 and U1 use custom footprints
with generic connector symbols; editing the embedded standard symbols to add
project-only filter names produced ERC library-mismatch warnings, so that
attempt was reverted. No electrical or layout data was changed for this rule
cleanup.

Refreshed V1.1 ERC/DRC JSON from the current project, documenting the two
exclusions in `hardware/c5-wio-carrier/v1.1/verification/README.md`. Created
`BOM-v1.1.csv` from the schematic plus the two Seeed modules and four
carrier-side female socket and module-side male header rails. Added exact
candidate part numbers for the unspecified passives, connectors, and header
strips. Case screws/supports are listed as enclosure-dependent rather than
inventing screw lengths. The Grove current budget and connector keep-clear
geometry remain unverified.

**Verified:** KiCad 10.0.6 ERC: 0 violations. DRC with schematic parity and
zone refill: 0 reported violations, 0 unconnected items, and 0 parity issues.
Three reviewed categories are active; two remain explicitly ignored for the
reasons above. These CAD checks do not qualify current margin, physical mating,
case fit, or fabrication.

## 2026-09-24 — Synchronize V1.1 PCB no-connect pad identities

**Phase:** Hardware tooling · **By:** Codex

Updated the V1.1 PCB's five isolated pads (U1 pin 11, J2/J3 pins 1 and 7)
from stale `NC_*` names to the corresponding KiCad-generated
`unconnected-*` identities used by the schematic. These pads had no routed
copper; board geometry, placement, routing, and silkscreen were unchanged.
Refreshed `hardware/c5-wio-carrier/v1.1/verification/drc-v1.1.json`.

**Verified:** KiCad 10.0.6 DRC reports zero violations, zero unconnected
items, and zero schematic-parity issues. This is CAD verification only; it
does not add physical qualification or fabrication approval.

## 2026-09-24 — Mark verified unused V1.1 contacts as no-connect

**Phase:** Hardware tooling · **By:** Codex + operator

Checked the Rev D unused-contact assignment against the P3 bench record in
`docs/hardware/lora-harness.md`. The real Wio harness was wired per Rev D and
successfully completed reset/SPI/radio reception without those four Wio
contacts; this is functional bench evidence, not individual continuity
measurements. Replaced the five isolated `NC_*` global labels (U1 pin 11, J2
pins 1/7, J3 pins 1/7) with explicit schematic no-connect markers. Kept the
PCB pads unconnected, using KiCad's generated `unconnected-*` identities to
preserve schematic parity. Recorded the selected prototype target as 2-layer
FR-4, 1.6 mm, 1 oz outer copper, lead-free HASL, black soldermask, and white
front silkscreen. No Gerber/drill files were exported.

**Verified:** KiCad 10.0.6 ERC reports zero violations. DRC with schematic
parity reports zero violations, zero unconnected items, and zero parity
issues. The selected stackup is an order target; manufacturer-specific DFM,
physical clearances, case fit, power margin, cold boot, and USB recovery remain
open gates.

## 2026-09-24 — Review V1.1 fabrication layers, ERC, and draft BOM

**Phase:** Hardware tooling · **By:** Codex

Generated black-and-white review plots for F.Cu, B.Cu, F.Mask, B.Mask,
F.Silkscreen, B.Silkscreen, F.Paste, B.Paste, and Edge.Cuts, plus a labeled
contact sheet and the schematic CSV BOM. Re-ran KiCad 10.0.6 ERC and DRC with
schematic parity. Left the five ERC labels unchanged for operator disposition.
Recorded that the schematic BOM is not purchase-complete: it has no supplier
part numbers, omits the Wio module as a schematic BOM item, and represents the
C5 socket strips only inside the custom U1 footprint/model.

**Verified:** ERC reports five single-use `NC_*` label warnings and zero
errors. Endpoints are U1 pin 11 and J2/J3 pins 1 and 7. Rev D confirms XIAO
D3/GPIO7 is intentionally unused and the Wio design leaves VIN/D0/D6/D7
unused; J2/J3 pin numbers alone are not Wio signal names. DRC reports zero
violations, zero unconnected items, and zero schematic-parity issues. The
review artifacts are under `hardware/c5-wio-carrier/v1.1/renderings/fabrication-review/`.
No PCB source, nets, outline, or manufacturing outputs were changed. Proposed
baseline for later approval: FR-4, 1.6 mm, 1 oz copper, lead-free HASL, black
soldermask, and white silkscreen. Connector insertion clearance was not
reviewed per operator direction; these checks do not qualify physical fit or
make the design ready to order.

## 2026-09-24 — Improve V1.1 connector-label readability

**Phase:** Hardware tooling · **By:** Codex + operator

Reoriented the left-side labels on the C5, Wio, and Grove connector symbols so
their names read outward from the pin rows rather than over the pin numbers and
symbol bodies. Kept the same named nets and pin endpoints; only label
presentation changed.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. ERC still has five warnings
for the intentional isolated `NC_*` labels. The exported schematic netlist
still matches the slim-study source exactly: 17 components and 20 nets.

## 2026-09-24 — Promote slim carrier and clean V1.1 schematic presentation

**Phase:** Hardware tooling · **By:** Codex + operator

Promoted the flush-fit slim layout into the standalone
`hardware/c5-wio-carrier/v1.1/` project, with matching V1.1 project, board,
schematic, local footprints, and STEP models. Reflowed the schematic into
functional sections and moved the mechanical datums clear of the title block.
Updated the project instance names and title-block revision. No PCB geometry,
electrical symbols, component values, or net connections changed.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. ERC retains five intentional
warnings for single-use `NC_*` labels on isolated contacts. Compared the
exported source and V1.1 netlists: all 17 components and 20 nets match exactly.
Generated the V1.1 assembly render and schematic PDF. These CAD checks do not
confirm physical module/header fit, enclosure access, Grove power margin,
cold-boot/USB-recovery behavior, or fabrication readiness.

## 2026-09-24 — Add Lens Core V1.1 silkscreen lockup to flush-fit study

**Phase:** Hardware tooling · **By:** Codex + operator

Replaced the `OSCILLA V1` board title in the isolated flush-fit study with the
repository's Lens Core mark from `docs/brand/assets/lens-core.svg`, the
`OSCILLA` wordmark, and a `V1.1` version label in the open upper-right corner.
The SVG's paths were traced as monochrome 0.15 mm F.SilkS vectors for
manufacturing; its focal dot is a filled silk circle. The brand guide contains
the wordmark as styled text rather than a separate wordmark SVG. No board
geometry, footprint placement, pads, nets, or routing changed.

**Verified:** The 3D top render shows the full mark and text clear of H1 and
nearby components. KiCad 10.0.6 DRC with schematic parity reports zero
violations, zero unconnected items, and zero parity issues. This is still a
mechanical study; logo silk legibility on a fabricated board and all physical
assembly tolerances remain unverified.

## 2026-09-24 — Center the male-header models on the module pad grids

**Phase:** Hardware tooling · **By:** Codex + operator

Compared all four 1×7 header STEP placements with the carrier pad centers in
the flush-fit study. The carrier arrays use 2.54 mm pitch. The C5 row pad 1
centers are at local Y=-2.88 mm; the Wio seven-pad row spans 15.24 mm. Corrected
the model offsets to C5 Y=2.88 mm on both rows and Wio Y=-15.24 mm on both rows.
This removed the previous 0.26 mm C5 offset and normalized the Wio rows that
had differed by 0.20 mm. PCB pad positions, nets, and routing were unchanged.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. The new orthographic 3D render
shows the module STEP models centered over the matching carrier pad grids.
STEP/render agreement does not qualify actual header tolerances, soldering,
physical module fit, or case clearance.

## 2026-09-24 — Add male-header spacer models to the flush-fit study

**Phase:** Hardware tooling · **By:** Codex + operator

Added standard KiCad 1×7, 2.54 mm vertical male-header STEP models to the two
C5 rails and two Wio rails in the isolated flush-fit study. The module models
are raised to show the spacer-body stand-off and the header pins passing through
the carrier. No pad, net, routing, or source slim-study changes were made.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. The renders show the intended
stacking concept, but exact header length, soldering method, physical module
fit, and enclosure clearances are not qualified by this model.

## 2026-09-24 — Create a flush-mount assembly study

**Phase:** Hardware tooling · **By:** Codex + operator

Copied the slim study to `hardware/c5-wio-carrier/flush-fit-study` so the
production-style assembly check does not alter the source layout. Removed the
carrier-side socket 3D bodies while retaining all pads and nets, lowered both
manufacturer module models to the carrier surface, and suppressed the two
female-header subassemblies included in the Wio STEP model in this copy only.
The supplied STEP models do not represent the planned replacement male pins,
so this first pass intentionally shows the bare module bodies and mounting
holes rather than claiming a complete final assembly.

**Verified:** The copied board's KiCad 10.0.6 DRC with schematic parity reports
zero violations, zero unconnected items, and zero parity issues. This confirms
the unchanged electrical layout only; the direct-mount stack-up, male-pin
geometry, soldering method, case clearances, and real-module fit remain open.

## 2026-09-24 — Move Grove connector toward the left edge

**Phase:** Hardware tooling · **By:** Codex + operator

Moved J1 2 mm left in the slim study, trimmed the local Grove footprint's
silkscreen outline to stay inside the board edge, and moved its reference label
clear of the connector pads. Added short pad connections and one layer-change
via to preserve the routed Grove nets, then refilled copper zones. The connector
housing model now sits about 1.05 mm inside the left board edge. C5 and Wio
bottom-row mounting pads remain about 2.08 mm and 2.13 mm from the bottom edge,
respectively; their modeled housing margins are tighter at about 1.80 mm and
1.73 mm.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. Plug insertion, enclosure
apertures, dimensional tolerances, and flush-mounted module fit still require
physical/case validation.

## 2026-09-24 — Slim-study connector and bottom-edge clearance review

**Phase:** Hardware tooling · **By:** Codex + operator

Reviewed the operator-adjusted STEP model placements in the slim study against
its Edge.Cuts rectangle (`x=102..160 mm`, `y=101.5..139.7 mm`). The exported
model envelopes put the C5 module's leftmost point about 1.18 mm inside the
left edge and its bottommost point about 1.80 mm inside the bottom edge. The
Grove connector envelope begins about 3.05 mm inside the left edge and has
25.25 mm of bottom-edge margin. The Wio module envelope has about 1.73 mm of
bottom-edge margin; the outer socket housing is about 1.71 mm from the edge.
No board geometry or model placement was changed during this review.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. These are STEP-mesh envelope
measurements only; the USB-C/Grove mating-plug insertion envelopes, case
apertures, tolerance, and real-module fit remain unverified.

## 2026-09-24 — Re-route the V1 carrier on two copper layers

**Phase:** Hardware tooling · **By:** Codex + operator

Preserved the prior manually routed V1 board as a rollback, then cleared tracks
and vias on an isolated copy and routed it locally with FreeRouting 2.4.1 using
KiCad's DSN/SES exchange. Kept the board outline, footprints, placement, and
schematic unchanged. Retained one continuous B.Cu GND pour and removed the
disconnected F.Cu GND pour; the router otherwise treated those front-side ground
islands as connected, while KiCad correctly reported an isolated zone item.
Added the Seeed XIAO symbol and footprint libraries and all 13 Seeed OPL
component footprint libraries to KiCad's global tables, leaving the carrier's
socketed-C5 and Grove footprints project-local.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. ERC remains at five intentional
isolated `NC_*` warnings. The route has 87 track segments and 8 vias on F.Cu and
B.Cu. Seeed's XIAO C5 DIP footprint and pinout reference match the carrier's
socket row sequence; Seeed's Wio-SX1262 schematic likewise matches J2/J3. These
are CAD/reference checks only. Smooth final channel grouping, physical socket
fit, USB/Grove access, standoff fit, measured power margin, and hardware testing
remain open. No fabrication data was exported.

## 2026-09-24 — Align slim-study silkscreen with moved C5 socket rows

**Phase:** Hardware tooling · **By:** Codex + operator

Moved the slim study's two C5 socket-outline rectangles upward 1 mm with the
socket rows, restoring their original clearance around the through-hole pads
without changing the module placement, electrical routing, or V1. Refreshed
the study's route and render artifacts.

**Verified:** KiCad 10.0.6 DRC with schematic parity now reports zero
violations, zero unconnected items, and zero parity issues. This is CAD
verification only; module 3D models, actual assembly fit, screw-head clearance,
and case fit remain open.

## 2026-09-24 — Refill V1 copper and start a slim placement study

**Phase:** Hardware tooling · **By:** Codex + operator

Refilled and saved V1's B.Cu GND zone without changing its outline or the
approved hole centers. Corrected the mechanical-review coordinates to match
the board file: H1 `(59.0, 3.2) mm`, H2 `(3.9, 16.8) mm`, H3 `(59.0, 37.6) mm`
relative to the upper-left. Exported both-side copper/mask and edge review
artifacts for the mask/clearance check. Created a separate local-only slim
study so experiments cannot move V1: its first candidate is 58.0 x 38.2 mm,
about 12.2% less rectangular area, with the left-facing Grove/USB service
edges and 15.24 mm module-row spacing retained. Shifted the module assembly
up together and adjusted the top service cluster and right-side mounts to the
new outline. Cleared only the study copy's old routes and rerouted it with
FreeRouting 2.4.1.

**Verified:** V1 KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues after the zone refill. The slim
study routes all nets; KiCad reports zero unconnected items and zero parity
issues, but 14 existing silk-over-copper clipping warnings remain. The slim
study's M2 screw-head/module clearance, cables, module removal, and case fit
are not physically verified. Neither board is cleared for ordering; no
fabrication data was exported.

## 2026-09-23 — Consolidate the two-layer carrier as V1

**Phase:** Hardware tooling · **By:** Codex + operator

Promoted the latest manual-routed two-layer candidate into the single active
local KiCad project at `hardware/c5-wio-carrier/v1/`. Matched the board,
schematic, project, and project-local footprints under one V1 name. Moved the
previous compact POC and two-layer experiments into `hardware/c5-wio-carrier/archive/`
without deleting their source files. Corrected the front/back GND-zone
boundaries to stay 0.5 mm inside the unchanged board edge.

**Verified:** KiCad 10.0.6 DRC with schematic parity reports zero violations,
zero unconnected items, and zero parity issues. ERC reports five warnings for
intentional isolated `NC_*` contacts. Generated the BOM, schematic PDF, 3D
assembly view, and separate top/front-copper/back-copper review images from V1.
These are CAD checks and review aids only. Module/socket fit, Grove and USB
access, standoff fit, measured power margin, and cold-boot/USB-recovery checks
remain open; no fabrication files were exported.

## 2026-09-23 — Remove duplicate and dangling manual GND copper

**Phase:** Hardware tooling · **By:** Codex + operator

On a new copy of the operator's manually routed board, removed one of two
co-located GND vias at `(153.5, 112)` and a 0.4 mm GND stub ending at
`(135.6, 119)`. The duplicate via was redundant at the same electrical
location; the stub had no connected endpoint. Preserved all other copper and
the C5 socket silkscreen boxes. KiCad 10.0.6 DRC reports zero PCB violations
and zero unconnected items. Schematic-parity checking remains unavailable
because the schematic is not fully annotated. The repaired result is a
separate review copy; the operator's original manual study remains unchanged.

## 2026-09-23 — Manual two-layer routing review and C5 socket silkscreen

**Phase:** Hardware tooling · **By:** Codex + operator

Reviewed the operator's saved `c5-wio-carrier-2layer-manual-study.kicad_pcb`.
Created a separate review copy with two F.SilkS rectangles around the C5
socket rows, matching the Wio socket-outline stroke and clearance; the
operator's saved manual-routing file remains unchanged. The render preserves
the manual copper and shows the paired outlines in place. KiCad DRC reports
zero unconnected items and two warnings on the existing GND routing: two
co-located vias and one 0.4 mm dangling track. Schematic-parity checking could
not run because the project schematic is not fully annotated. Keep the review
copy as a proposal until the operator accepts the appearance and we resolve the
ground warnings together.

## 2026-09-23 — Two-layer SPI center-channel experiment

**Phase:** Hardware tooling · **By:** Codex + operator

Tested an ordered, parallel center run for the C5-to-Wio SPI trio with 45°
fanouts. The coarse-grid autorouter could not preserve the proposed bundle and
also keep the UART/control/power routes and ground references clean: its
best-placed trial produced six crossing violations, while earlier placements
blocked ground routing. Discarded the experimental bundle and regenerated the
study from the unchanged compact source. The retained two-layer board is the
prior 138-segment, 19-via route; KiCad 10.0.6 DRC with schematic parity reports
zero violations, unconnected items, or parity issues. The requested visual
channel cleanup remains open and needs deliberate route editing rather than
this generic autorouter; do not treat the restored baseline as the organized
final routing.

## 2026-09-23 — Two-layer carrier channel-routing direction

**Phase:** Hardware tooling · **By:** Codex + operator

Reviewed the combined render against the operator's request for smooth,
understandable routing channels with center runs. The current route is an
experimental autoroute and still does not meet that visual/organizational goal.
One cleanup experiment removed explicit GND tracks to reduce clutter; KiCad
then reported six unconnected ground items, so that version was discarded and
the grounded route restored. The restored two-layer study has 138 segments and
19 vias; KiCad 10.0.6 DRC with schematic parity reports zero violations,
unconnected items, or parity issues. This is electrical connectivity evidence,
not evidence of polished routing or physical qualification.

Next routing pass should define parallel bundles for SPI and control signals,
give each group a deliberate center corridor and fanout, and preserve the GND
reference. Keep C1/C2 beside the Wio power pins. Do not treat the autorouter's
crossing-free result as the final channel layout.

## 2026-09-23 — C5/Wio carrier: two-layer route organization review

**Phase:** Hardware tooling · **By:** Codex + operator

Rebuilt the isolated study from the untouched compact board rather than
stacking tracks onto a prior router output. Fixed the experimental helper so
it removes both KiCad multi-line tracks and its own one-line output before a
rerun. Tried the operator-noted open upper area for the three pull resistors;
it increased detours, so restored them to a vertical bank beside the Wio
headers. Kept C1/C2 at the Wio power pins. KiCad 10.0.6 DRC with schematic
parity now reports zero violations, unconnected items, or parity issues. The
3D render overlays front and back copper and looks tangled; exported separate
layer views for review. The automatic route remains visually busy and is not
considered organized or release-ready; a deliberate bus/layer cleanup is
still needed. This is a two-layer study only; the parent four-layer baseline
is unchanged.

## 2026-09-23 — C5/Wio carrier: reroute two-layer study and use open component area

**Phase:** Hardware tooling · **By:** Codex + operator

Continued the isolated two-layer study without modifying the active compact
four-layer baseline. Rebuilt the routes across F.Cu/B.Cu to eliminate the
crossings from the mechanical layer-map experiment. Used the operator-noted
open upper-right area for R1-R3; kept C1/C2 local to the Wio power pins rather
than moving bypass parts away from their loads. Current KiCad 10.0.6 DRC with
schematic parity reports zero unconnected items, zero parity issues, and zero
copper crossings. Two B.Cu copper-sliver warnings remain. The experimental
coarse-grid route (136 segments, 19 vias) is visually reviewed but remains a
POC, not a fabrication candidate; route cleanup, the two warnings, physical
module/connector fit, and independent release checks remain open. See the
study README and `autoroute-pass-4-drc.txt` for the current scope and evidence.

## 2026-09-23 — C5/Wio carrier: first two-layer routing feasibility probe

**Phase:** Hardware tooling · **By:** Codex + operator

Created a separate two-layer study copy; the active compact four-layer board,
schematic, and source project remain unchanged. The first-pass mapping moves
In1.Cu routes to B.Cu and In2.Cu routes to F.Cu without changing component
placement. KiCad DRC with schematic parity reports 16 copper crossings, one
solder-mask bridge, one dangling via, and two project-local footprint-library
warnings; there are zero unconnected items and zero schematic-parity issues.
This is a routing feasibility probe only, not a completed two-layer reroute or
a fabrication candidate. See `hardware/c5-wio-carrier/two-layer-study/` for
the board copy, render, and DRC report.

## 2026-09-23 — C5/Wio carrier: trim lower edge and check silk over routing

**Phase:** Hardware tooling · **By:** Codex + operator

Confirmed the TP reference labels do not touch exposed pads; their apparent
overlap with copper in the 3D review is routed copper shown through the board
stack/under solder mask. DRC reports no violations. Trimmed the compact board's
lower Edge.Cuts boundary from y=144 mm to the operator's green guide at
y=140.7 mm, creating a 62 x 40.7 mm outline. Rerouted the lower GND,
LORA_RESET, LORA_BUSY, and LORA_RF_SW paths to y=139.5 mm and moved H3 up 1 mm
to leave 1.1 mm of board beyond the 2.2 mm mounting-hole rim. Updated the
documented hole coordinates and envelope.

DRC with schematic parity reports zero violations, unconnected items, or parity
issues. The modules remain absent from the 3D assembly view, so USB-C/Grove
clearance still needs a physical fit check; this remains a mechanical POC.

## 2026-09-23 — C5/Wio carrier: bring the left edge to the connector line

**Phase:** Hardware tooling · **By:** Codex + operator

Moved only the compact board's left Edge.Cuts boundary 2 mm inward, from
x=100 mm to x=102 mm, following the operator's green guide. The new outline is
62 x 44 mm. The nearest existing routed copper is the inner-layer +3V3 segment
at x=104 mm. Rerouted the Grove 5 V protection path on B.Cu from x=102 mm to
x=104 mm, leaving a 2 mm edge setback. Replaced the clipped footprint J1 field
with on-board silk above the connector. Rebased mounting-hole coordinates
to the new top-left corner; absolute hole positions are unchanged.

Rendered and reran DRC with schematic parity after the outline, route, and label
changes. This is a plan-view fit adjustment only; verify Grove and C5 USB-C
mating access against the purchased connectors and printed case before
freezing the outline.

## 2026-09-23 — C5/Wio carrier: restore F1 body in render

**Phase:** Hardware tooling · **By:** Codex

The F1 footprint referenced `Fuse_1812_4532Metric.step`, which is not present
in the installed KiCad 3D model library; renders therefore showed only its
pads. Pointed the 1812 PPTC visualization to KiCad's available 1812 chip-body
model. This changes the review rendering only; F1's footprint, value, and
electrical connections are unchanged. The chip model is a package-shape proxy,
not a manufacturer-accurate rendering of the Bourns part.

Regenerated the compact board 3D render and verified that a body is visible
over F1. This does not establish component clearance or assembly fit.

## 2026-09-23 — C5/Wio carrier: Grove proxy and C5 row labels

**Phase:** Hardware tooling · **By:** Codex + operator

Attached a standard KiCad 4-pin, 2.00 mm right-angle connector model to J1 so
the Grove connector body appears in the board render. This is a visual proxy,
not the exact Seeed 1125R-4P body; its local planning footprint still needs
vendor-drawing or physical verification. Added silk-only J4/J5 labels beside
the two C5 socket rows while preserving U1 as the schematic's 14-contact
module interface. Confirmed F1 already has the planned Bourns
`MF-MSMF125/16X` value and a generic 1812 fuse 3D body in the render.

KiCad 10.0.6 DRC with schematic parity reports zero violations, zero
unconnected items, and zero parity errors. This edit changed 3D/assembly
graphics and silkscreen labels only; no electrical nets changed.

## 2026-09-24 — C5/Wio carrier: add module STEP models to slim-study render

**Phase:** Hardware tooling · **By:** Codex + operator

Added the operator-supplied Seeed STEP references to the local slim study and
linked the C5 model to U1 and the Wio-SX1262 model to J2 using project-relative
`${KIPRJMOD}` paths. Refreshed the study's 3D render and ran KiCad DRC again.
The model offsets are provisional; the operator will refine their placement in
KiCad's footprint 3D Models tab. This visualization work does not move copper,
pads, or module footprints, and the model placement is not physical-fit proof.

**Verified:** KiCad 10.0.6 render completed; DRC reports zero violations and
zero unconnected items. C5/Wio pin alignment, C5 USB-C orientation, antenna
connector clearance, and standoff/case fit remain open for review.

**Phase:** Hardware tooling · **By:** Codex + operator

Added two 1x7 female socket 3D models to the socketed C5 carrier footprint,
positioned on the 14 C5 through-hole contacts. This makes the carrier-side
socket arrangement visible alongside the Wio sockets in the review render.
Male headers remain soldered to the removable C5 module; the render does not
yet include the C5 or Wio module bodies themselves. The generic KiCad socket
models communicate the connector arrangement, not a selected vendor socket's
exact height or body shape.

KiCad 10.0.6 DRC with schematic parity reports zero violations, zero
unconnected items, and zero parity errors. The fresh 3D render was inspected.
No electrical nets or board geometry changed.

## 2026-09-23 — C5/Wio carrier: align module rows and move H2

**Phase:** Hardware tooling · **By:** Codex + operator

Moved H2 beside the left side of the C5 to match the operator's marked
location, moved its silk reference clear of the Grove footprint, and shortened
the board from 64 x 48 mm to 64 x 44 mm. The C5 and Wio socket rows now share
the same horizontal centerlines (122.48 mm and 137.72 mm in board coordinates)
and each 1x7 row uses 2.54 mm pin pitch. The operator confirmed by testing
that C5 and Wio pin positions are 1:1-compatible. U1 retains two 1x7 through-
hole socket rows for the user's C5 male-header test arrangement; the 1.0 mm
drill / 1.8 mm land are still prototype assumptions.

KiCad 10.0.6 DRC with schematic parity reports zero violations, zero
unconnected items, and zero parity errors. ERC reports the five intentional
isolated `NC_*` warnings. The fresh board render was reviewed. Board-envelope
arithmetic is 26.7% less plan area than the 80 x 48 mm draft. No enclosure CAD
or physical fit evidence is available yet.

## 2026-09-23 — C5/Wio carrier: update socket geometry to XIAO mating grid

Moved the Wio socket rows from 17.78 mm to 15.24 mm center-to-center while
preserving their pair midpoint and 2.54 mm pin pitch. The spacing follows the
official Seeed XIAO ESP32-C5 DIP footprint and the Wio's stated XIAO
compatibility; Seeed's accessible documentation does not give a numeric
row-spacing callout for the exact Wio carrier SKU, so that remains pending
physical cross-check. Added two socket-strip assembly outlines to the C5's
embedded and project-local footprints, keeping copper pads and module centers
unchanged. Reduced the C5 socket lands from 2.0 mm to 1.8 mm (still a 0.4 mm
annular ring around the 1.0 mm assumed drill); this provides clearance for the
existing direct SPI traces without changing the published pad centers. Moved
the flexible lower-centre M2 point to `(28, 44)` mm from the board's top-left
and extended the bottom edge 4 mm (to a 64 x 48 mm envelope) so it clears the
C5/Wio courtyards and reset trace while retaining 4 mm of board-edge margin.

**Verified:** KiCad parsed the board and produced a fresh render. DRC reports
zero violations, zero unconnected items, and zero schematic-parity issues. ERC
reports the expected five intentional isolated `NC_*` label warnings. The
board is still a layout POC, not order-ready, and physical module fit is
unverified.

## 2026-09-23 — C5/Wio compact carrier: footprint-cache and silk cleanup

**Phase:** Hardware tooling · **By:** Codex + operator

Adapted the socketed C5 footprint to Seeed's published XIAO ESP32-C5 DIP body
outline and 2.54 mm pad grid (15.24 mm row spacing). The carrier's 1.0 mm
socket drill and 2.0 mm land remain prototype assumptions. Adjusted the C5
escape endpoints to the revised hole centers without changing their net
assignments. Refreshed all embedded standard-footprint copies from KiCad 10.0.6
and both custom copies from the project libraries. Removed cramped explanatory
front-silkscreen notes; assembly and case intent remain documented in the
README/mechanical review. Marked schematic test points and mounting datums
out-of-BOM to match their standard footprint attributes.

**Verified:** KiCad 10.0.6 render succeeds; DRC reports zero unconnected items,
zero schematic-parity issues, no footprint/library mismatches, and no silk/text
violations. One DRC error remains: the C5 module courtyard overlaps the
lower-centre M2 hole courtyard. ERC still reports the five intentional isolated
`NC_*` contacts. The Grove footprint is not yet vendor-outline-verified; the
board remains a POC and is not ready to order.

## 2026-09-22 — C5/Wio carrier: populated routed-POC start; DRC blocks release

**Phase:** Hardware tooling · **By:** Codex + operator

Placed the C5 SMD castellated-pad pattern, socketed two-row Wio interface,
Seeed-keyed 1125R-4P Grove connector, F1/D1 Grove-power protection, required
pulls/bypass, three power test pads, and M2 case datums onto the 80 x 48 mm
carrier. The first route pass connects the intended Rev D net set visually,
but it is **not an electrical release**: KiCad reports 12 open items and
copper/mask conflicts. Inspection caught the key cause—the first left-side C5
routes crossed the module's right-side pads. The next route must escape around
the C5 pad edges and use the inner layers deliberately, then gain an annotated
schematic for ERC/parity DRC. No fabrication output was generated.

## 2026-09-22 — C5/Wio carrier: conservative Grove-power POC estimate

**Phase:** Hardware tooling · **By:** Codex + operator

Converted the requested Grove-powered concept into a deliberately bounded POC
power plan. The only series topology is `Grove red → F1 → D1 → C5 5 V`, with
the diode oriented to prevent USB-to-Grove back-feed. Reserved a Bourns
`MF-MSMF125/16X` 1812 PPTC (1.25 A hold / 2.50 A trip at 23 °C; 1.00 A hold at
40 °C) and a Diodes Inc. B240A-class 2 A/40 V SMA Schottky. The revised
mechanical board shows this as a placement reservation, not routed copper.

The estimate uses Rev D's provisional 0.91 A / 3.3 V combined-margin load:
3.00 W. At an assumed 80% conversion efficiency that is 0.75 A from 5 V;
with an additional 25% POC allowance the planning source load is 0.94 A,
rounded to **1.0 A**. This does not prove Cardputer availability, C5 regulator
thermal behavior, diode temperature, or USB/Grove isolation. P6 remains the
release gate; no fabrication output or Grove-only authorization was produced.

## 2026-09-22 — C5 + single-Wio co-mounted carrier started (not release-ready)

**Phase:** Hardware tooling · **By:** Codex + operator

Operator selected a co-mounted C5/Wio carrier with a Grove input, superseding
the initial USB-only connector disposition. The first KiCad 10 routing draft
used Rev D's custom Wio signal map and a 0.75 A-hold PTC plus SS14. It was
subsequently archived as an unverified experiment in
[`hardware/archive/c5-wio-carrier-routing-draft-2026-09-22`](hardware/archive/c5-wio-carrier-routing-draft-2026-09-22/),
not carried forward as the carrier design. No fabrication output was produced
or approved; module fit, antenna clearance, boot/USB recovery, and P6 power
headroom stay physical gates.
# 2026-09-22 — C5/Wio carrier: archived routing experiment; added fit-first floorplan

- Moved the unverified 90 x 68 mm manual-routing experiment to
  `hardware/archive/c5-wio-carrier-routing-draft-2026-09-22/`; it remains a
  record only and is not a fabrication candidate.
- Added `hardware/c5-wio-carrier/` as a clean 80 x 48 mm mechanical KiCad
  floorplan. It reserves separate C5 USB-C access, Wio antenna clearance,
  Grove edge ingress, and four candidate M2 locations while fitting inside
  the Cardputer ADV's published 84 x 54 mm plan envelope.
- This is deliberately not an electrical release: no final module footprints,
  schematic, selected Grove connector, selected 5 V protection, or routed
  netlist exists yet. Case alignment, P6 power current, USB/Grove isolation,
  and C5 cold-boot/native-USB recovery with GPIO25 attached stay as physical
  gates.
- Operator clarified the supplied C5 orientation: USB-C is at the module's
  top edge. The 80 x 48 mm floorplan now places that edge at the case opening.
  Both radios use IPEX/u.FL coax leads to external case-mounted antennas, so
  the Wio antenna bay was removed. The four 2.2 mm M2 centers form a 70 x
  38 mm rectangle, 5 mm in from each board edge, and now define the enclosure
  standoff datum; the future case is designed from the PCB.
- Replaced the oversized generic Grove service box with the selected compact
  Seeed `1125R-4P` 90-degree, 2.00 mm connector reservation. Added the
  explicit C5/Wio/Grove electrical contract and verified official C5 SMD
  footprint availability. Grove 5 V remains routed only through an as-yet
  unselected protection stage pending P6 current and back-feed qualification.
## 2026-09-23 — C5/Wio carrier: compact 64 x 44 mm placement POC

**Phase:** Hardware tooling · **By:** Codex + operator

Kept the user's Grove-powered, co-mounted C5/Wio concept but reduced the
placement envelope from 80 x 48 mm to **64 x 44 mm** (26.7% less area). The
compact POC preserves the top-edge C5 USB-C case opening, two Wio header rows,
the keyed 1125R-4P Grove input, the F1/D1 POC power stage, required pull and
local bypass network, power test pads, and four M2 case datums. The lower-left
mounting point is intentionally moved inboard to make room for the Grove body
and cable path; the enclosure must use the published coordinates rather than
assume a symmetric pattern.

KiCad 10.0.6 loads and renders the compact board. DRC is **not a release
gate**: 29 unconnected items remain because this is a placement POC, and
schematic-parity DRC correctly rejects the missing annotated schematic. The
non-net warnings are draft silkscreen/library issues; no fabrication outputs
were generated. Next work is to produce the authoritative schematic, verify
the purchased Wio outline/headers and cable geometry, then route the compact
board from the C5 pad edges and run ERC plus parity DRC.
## 2026-09-23 — Compact carrier: top-left mount removed for C5 service space

**Phase:** Hardware tooling · **By:** Codex + operator

Removed the compact POC's top-left M2 hole at the operator's request. The
enclosure datum is now a three-point triangle: upper-right `(59.5, 5.0)`,
lower-centre `(28.0, 40.0)`, and lower-right `(59.5, 39.5)` mm from the compact
board's top-left. This is a mechanical POC decision only; case stiffness, USB
cable load, and actual module clearance remain physical validation gates.
## 2026-09-23 — Compact carrier: left-side service and top-case antennas

**Phase:** Hardware tooling · **By:** Codex + operator

Reoriented the compact placement POC around the intended printed-case use:
the Grove connector is at the upper-left with its mating mouth facing left,
and the C5 is rotated so USB-C also exits through the left case wall. The Wio
is now horizontal at the lower-right. The upper board strip is reserved for
the two module u.FL leads to the operator's top-case SMA bulkheads; no SMA or
antenna footprint was added to the PCB. The C5/Wio electrical net assignment,
the required pulls, and local bypass values are unchanged. This is still a
placement-only mechanical POC: validate cable boots, USB plug clearance,
u.FL bend radius, and case-wall/antenna geometry on the physical assembly.
## 2026-09-23 — Compact carrier: rotated C5 pad geometry repaired

**Phase:** Hardware tooling · **By:** Codex + operator

The first left-service placement render revealed that the hand-carried C5
footprint rotated pad positions without rotating the pads' long axes. Adjacent
castellated pads consequently overlapped, producing shorts, clearance, and
solder-mask-bridge DRC findings. Added the required 90-degree pad orientation
to the C5 footprint's fourteen castellated pads, then re-rendered and reran
DRC. Those copper findings are gone. Remaining DRC output is the expected
29 unconnected items plus missing schematic-parity, draft silkscreen, and
local-library warnings; this remains a placement POC, not a release.
## 2026-09-23 — Compact carrier: C5 changed from soldered to removable

**Phase:** Hardware tooling · **By:** Codex + operator

Replaced the carrier's C5 SMD/castellated land pattern with fourteen
through-hole contacts that follow Seeed's official C5 pad-center geometry.
The first board now expects two 1x7 male headers soldered to the C5 and two
matching female sockets on the carrier, mirroring the removable Wio approach.
All Rev D nets—including Grove 5 V protection, UART, SPI, control, pulls, and
local bypass—are unchanged. This makes swap/reflash/bench diagnosis practical,
but the printed case must retain the raised C5 without transmitting cable loads
through the headers. Socket current, USB-left-wall fit, and retention are
physical gates, not established by the KiCad change.
## 2026-09-23 — C5/Wio carrier: annotated schematic and parity baseline

**Phase:** Hardware tooling · **By:** Codex + operator

Added `c5-wio-carrier-compact.kicad_sch` for the Grove input/protection,
socketed C5, both Wio header rows, pulls, bypass capacitors, test pads, and
three case datums. Its component references, footprints, pin numbers, and net
names now match the routed board; local C5 and Grove footprint libraries are
included. Each intentionally open module contact uses its own isolated
`NC_*` net with no routed copper so KiCad can compare it explicitly.

**Verified:** KiCad 10.0.6 ERC reports only five `isolated_pin_label` warnings
for those intentional single-contact NC nets. Board DRC reports zero
unconnected items and zero schematic-parity issues, and no copper shorts,
clearance errors, crossings, or dangling tracks. DRC still reports POC
silkscreen/text findings and simplified-footprint/library geometry mismatches;
the board is not ready for fabrication. Schematic and board renders were
reviewed, and `git diff --check` passes.

## 2026-09-23 — C5/Wio carrier: compact routing POC

**Phase:** Hardware tooling · **By:** Codex + operator

Routed the compact 64 x 44 mm carrier without changing the agreed module,
Grove, USB-service, antenna, or case-datum placement. The POC carries the
Grove 5 V protection chain, both UART directions, 3V3/GND distribution, all
eight required C5-to-Wio control/SPI nets, the three startup pulls, local
bypass, and test-pad feeds. The route separates the SPI bundle and control
escapes across the available copper layers and keeps the top strip open for
the two u.FL leads. KiCad's board DRC now reports no short, clearance,
crossing, or dangling-track errors. It still cannot run schematic-parity
checks because the annotated schematic does not yet exist; no fabrication or
physical qualification claim follows from this routing pass.

## 2026-09-23 — C5/Wio carrier: socket label collision removed

**Phase:** Hardware tooling · **By:** Codex + operator

Moved the C5 reference designator (`U1`) from front silkscreen to the
fabrication layer so it no longer overlaps the visible `SOCKETED C5` label.
The latest placement render confirms the C5 socket label is legible. This is
visual POC cleanup only; it does not close electrical, mechanical, or
enclosure-fit gates.

## 2026-09-23 — C5/Wio carrier: add C5 socket models to assembly render
