# Hardware revision worklog

PCB, electrical, mechanical, enclosure, and carrier revision history. Newest
entries first. Real-hardware measurements and qualification remain in their
respective bench records under this directory; this log records design changes
and their validation status. See [Rev D](../../Research/c5-backpack-design.md)
for electrical authority.

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

## 2026-09-23 — C5/Wio carrier: add C5 socket models to assembly render

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
