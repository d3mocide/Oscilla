# Hardware revision worklog

PCB, electrical, mechanical, enclosure, and carrier revision history. Newest
entries first. Real-hardware measurements and qualification remain in their
respective bench records under this directory; this log records design changes
and their validation status. See [Rev D](../../Research/c5-backpack-design.md)
for electrical authority.

## 2026-11-22 — External display stays lit in idle sleep; power-gate harness proposed

- **Observation (operator, first flash of WP-3..12b):** idle sleep works, but the
  GMT028-05 external panel turns white instead of dark. Cause: Rev D §5.4 panel
  has no backlight pin, so `DISPOFF`/`SLPIN` stops pixel drive while the
  backlight stays powered. Battery cost not yet measured.
- **Proposed:** [`display-power-gate.md`](display-power-gate.md) — S3 GPIO4
  (EXT3) `DISP_EN` driving a high-side switch on the panel VCC branch plus a
  74LVC126 that tri-states SCK/MOSI/DC/CS/RST, so the unpowered panel cannot load
  the shared SD lines. Status: proposed, not built; Rev D unchanged.

## 2026-11-16 — GMT028-05 V1.1 replacement display selected

- User photos establish the seven-pin header: GND, VCC, SCK, SDA, RST,
  DC, CS. This differs from the old eight-pin no-CS ILI9341. Rev D §5.4
  owns the label-to-EXT/GPIO mapping; CS is wired to GPIO5/EXT13.
- User reports 3.3 V supply operation. The ST7789 standalone fixture
  produced colours, and the native-driver normal firmware now renders
  the interface. User confirmed correct colours after `invert=false`.
- Firmware supports only this replacement panel going forward. Shared-SPI
  testing at 8 MHz passed 32 write/read/cleanup cycles (128 KiB), with clean
  output confirmed by the user. Details are in the software WORKLOG;
  physical cold-boot/soak and combined power-budget gates remain open.

## 2026-11-16 — Second test print: the hinge bar cover could not be closed

**Phase:** Hardware tooling · **By:** Claude + operator

The operator reprinted bar and cover: the antenna now seats, but the cover does not go on. Two faults, both in the cover (C1), both mine:

- **Cover drawn 1.4 mm off along the bar.** `hinge_bar.py` placed the original cover file by eye (x + 0). Its screw boss is at
  x 23.1 in the file, the bar's screw hole at x 21.7; the true place is x - 1.4, which also puts the cover exactly between the
  hinge blocks (8.6 to 67.9; L1 ends 8.5, R1 starts 68) and its lip and hook in the bar's channel and slot. So on the printed
  cover the new plate and rims sat 1.4 mm off the original boss, lip and hook. `C1_AT` is now (-1.4, 7.67, 15).
- **The cover could no longer be put on at all.** The original cover tilts in about its hook (a barb under a ledge of the bar at
  x 54.4 to 62.9, pointing toward the Cardputer). The deeper plate reaches 4.8 mm past that hook and lands on the new outer wall
  before the barb is under the ledge. The new cover goes **straight down 1.1 mm outward of its place, then slides in** until the
  hook is under the ledge, then the screw (`slide`). For that: barb cut back from z -7.6 to -8.0 (0.5 to 0.9 mm under the ledge),
  a flat on the screw boss (1.1 mm of wall left at the hole), rims set 1.3 mm back from the outer wall (the narrow one over the
  coax hollow is now a solid strip).

The bar (S1) is unchanged: same mesh as the second print. Only C1 is to be reprinted. Second-print files are kept in
`enclosure/screen-gnss/out/print2-2026-11-16/`.

**Verified:** the script now steps the cover along that path (down at z - 1.1, then in) against the new bar: no overlap beyond
the seated pair's 0.25 mm3 (the original pair, correctly placed, has 0.17); lifted 0.5 mm when seated it collides (the hook
holds); no overlap with L1, R1 or the antenna envelope. Sections in `out/preview-cover-fit.png` (`python3 coverfit.py`).
Sliced in OrcaSlicer 2.5.0, no supports, 1.3 g, about 10 min.

**Not verified:** not printed. What the operator saw ("the lid is upside down") was not confirmed against these two faults by
photo. Against sliding back out the cover is held only by the screw and by friction; if it creeps, it needs a detent.

## 2026-11-15 — First test print: coax throat in the hinge bar, GNSS spot moved to the hinge edge, raised mark

**Phase:** Hardware tooling · **By:** Claude + operator

The operator printed the hinge bar, cover and the finned screen tub, and keeps the finned tub as the design. Three findings:

- **Antenna in the bar:** it has to lie ceramic face toward the cover, which puts its board, shield can and coax on the pocket
  floor; the only way out of the pocket was a notch at the top, so it could not seat. `hinge_bar.py`: the notch is now a throat
  open from floor to top (`throat`, 5.0 mm from the outer wall; 3.5 mm where it meets the coax hollow). The 1.6 mm left of the
  pocket's end wall still stops the antenna. The cover rim is opened to match.
- **GNSS board position:** the coax did not reach the middle of the back. The pocket moves toward the hinge edge, same x:
  `gps_fins.py` z 27 -> 12 (block from the fin at z 3 to the fin at z 21, pocket 3.6 mm above the tub's inner bottom wall); `gps_pocket.py` z 27 -> 13 (brim 1.6 mm from the bottom lip).
- **Oscilla mark:** sunk 0.6 mm with 0.8 mm lines it printed muddy. It moves to the top centre (z 39) and is raised like the
  fins: a hexagon ring one fin wide, and inside it the two ellipses (1.4 mm lines) and a centre ring (5.2 mm across, 3.0 mm eye) standing full fin height
  from the back plate. The ring covers the whole crossing so no slivers are left; a first try with a solid hub lost the circle (operator). Same 6 mm grid: the hexagon's bottom corner is
  on the centre fin, its top corner on the top border, gaps end square on its flats or on a slant.

- **Outer grooves:** the three square grooves round the bar (0.6 x 0.5 mm) printed soft: three layers and an overhang each.
  Bar and backpack tray now have two V-grooves instead, 1.4 mm wide and 0.6 mm deep (about 40 degree sides, no overhang),
  1.4 mm apart (bar y 3.1 and 5.9, tray Z 5.3 and 8.1). The bar wall is 0.6 mm at the tip of each V. Tray 17.0 g, about 126 min.
- **Screen tub walls (finned version only):** the original Cap TFT tub has four V-grooves 0.8 mm wide and 0.4 mm deep round its
  outer walls (y 1.8, 3.4, 5.0, 6.6), too fine to print well. `gps_fins.py` now fills them and cuts three V-grooves of the bar's
  size (1.4 x 0.6 mm) at y 3.0, 5.8 and 8.6, the third in the new thicker back. The side wall is 0.7 mm at the tip of a V.
  The V is built in eight steps, finer than a print layer. A first build ran them round the whole outline, hinge legs included; the operator wanted them where the original's are, so they are clipped to z >= 18 (up both sides and across the top, measured on the original). `gps_pocket.py`, D2, D3, L1 and R1 are unchanged.
- **Bar right end (operator, in the slicer):** the end wall of the deeper section came out as loose pieces. The original
  bar's outer face is chamfered from x = 66 to 68, so the new end wall (x 66 to 67.2) met it along one line only; the first
  print held by its floor and outer wall, the deeper V-grooves cut it free. A wedge now fills the space under the chamfer and
  the grooves stop where the old face begins (z = -10.07). Checked in sections at x 65 to 67 and y 0.2 to 7.
- **Mark, second pass:** the operator wanted the centre circle whole; the solid hub became the ring described above.

**Verified:** all solids valid, no overlap with the antenna or GNSS board envelopes; sliced in OrcaSlicer 2.5.0 without supports
(bar 4.7 g, cover 1.3 g, finned tub 19.6 g, about 153 min; two-part tub 11.6 g + cap 0.9 g). Mark checked in a close-up section.

**Not verified:** not reprinted. Where the coax leaves the antenna across its width was read from a photo, not measured. The
"GNSS" label is still sunk 0.4 mm and may print as poorly as the old mark. Files of the first print are kept in
`enclosure/screen-gnss/out/print1-2026-11-15/`.

## 2026-11-15 — Carrier v1.5 (slimmer), finned one-piece screen back as an alternative

**Phase:** Hardware tooling · **By:** Claude + operator

- **Carrier v1.5** (`hardware/c5-wio-carrier/v1.5/`, from v1.4 by `make_v15.py`): the operator marked up the v1.4 render: logo
  to the right and smaller, Grove jack up, less height. Board 79.0 x 35.0 -> 79.0 x 30.1 mm; logo at 80 % under the Grove jack;
  J1 3.7 mm higher; plain rectangular outline. v1.5 is now current and the backpack shell is refitted to it.
- **Screen tub, second option** (`enclosure/screen-gnss/gps_fins.py`): the operator asked whether the raised GNSS pocket needs
  supports and whether fins or a thicker back would print more easily. Neither option needs supports. The new one-piece
  version grows the whole back by 3.2 mm as nine fins (1.6 mm, 3.6 mm gaps) with a solid block for the pocket; the fins touch
  the bed and the original back plate bridges the gaps. Display screws keep their length through counterbores. The two-part
  version (`gps_pocket.py`: window plus hat-shaped cap) stays as the lighter option.
- The operator reports standard SMA bulkheads, and that the Wio takes the same space as the C5.

**Verified:** v1.5 ERC 0 violations; DRC (all severities, zone refill, schematic parity) 0 violations, 0 unconnected, 0 parity
issues. Both tub versions: solids valid, no overlap with the GNSS board envelope, slice in OrcaSlicer 2.5.0 without supports
(two-part: tub 11.6 g + cap 0.9 g, about 97 min; finned: 19.2 g, about 149 min, generic profile).

**Not verified:** nothing fabricated, printed or fitted. Hinge behaviour with the heavier finned back is not assessed.

**Later the same day:** the operator keeps both tub versions and asked for the Oscilla mark in the fin pattern and a larger
board logo. The finned back now has a solid hexagon (12 mm corner radius) in the middle with the mark sunk 0.6 mm; the fins run
into it. Its new layer is built from the tub's full outline: the first build followed the panel lines and left the two top
corners without a layer, which would have printed in mid-air. Finned tub now 19.9 g, about 155 min, no supports; nothing but
the fin gaps is bridged. Carrier v1.5 regenerated with the logo at 92 % (was 80 %), same outline; DRC still 0 / 0 / 0.

**Symmetry pass (operator markup + harness photo):** the fin grid, hexagon and GNSS block had been placed independently, which
left stub slots and uneven fin ends. The finned back is now laid out from the centre of the back plate (x 142, z 27): a solid
nameplate band 24 mm tall with the mark in the middle and two identical 16.8 x 14.8 pockets 20 mm either side (GNSS on the
wire-entry side, a spare marked PWR on the other), and three equal gaps above and below (1.4 mm fins, 2.6 mm gaps, 3.0 mm
border top and bottom). The operator's photo shows the harness running across the top third and down the wire-entry edge; the
GNSS board was about 1 mm from it, so in both tub versions it moved 1 mm inboard (2.6 mm clear on paper). Finned tub 19.8 g,
about 154 min, no supports.

**Grid pass:** the operator preferred fins that cut round the shapes, with a single GNSS spot and no twin pocket. The
nameplate band is dropped. Fins, hexagon and block now share one grid centred on the back plate: 6.0 mm pitch (1.8 mm fins,
4.2 mm gaps) with a fin on the centre line and four gaps above and below; the solid hexagon's corner radius is two pitches
(12 mm), so each gap clears it, ends square on a flat or ends on a slant, and its top and bottom corners land on fins; the
GNSS block is as tall as the hexagon, starts behind it at the centre line and runs to the end of the gaps. No stub slots or
slivers remain. 19.6 g, about 152 min, no supports.

**Mark fix:** in that build the vertical ellipse of the mark was missing inside the horizontal one (operator spotted it). The
cause was the 2D ring construction (rotating an ellipse, then a 2D offset). The rings are now drawn directly as an outer
ellipse minus an inner one, joined as solids and cut once. Checked in a close-up section: both ellipses are whole.
The backpack lid had the same fault (operator spotted it; an earlier note here-abouts that it was fine was wrong) and its
hexagon was turned 90 degrees from the brand mark. Both fixed the same way and checked in a close-up section.

**Before the first test print:** operator measured the Wio at 6 mm; on 2.5 mm header spacers that is the 8.5 mm the backpack
allows (0.5 mm to the lid). He reports the Cardputer model matches his unit and the antenna coax reaches. Hinge bar styled
to match the backpack: three fin grooves round its outer face (0.6 x 0.5 mm at 1.4 mm pitch, as on the tray) and a 0.5 mm
chamfer on the cover's outer edges. Bar and cover still slice without supports (4.8 g + 1.3 g, about 50 min). First test
print planned: hinge bar, cover and screen tub.

## 2026-11-15 — Carrier v1.4 (four corner screws, visible logo), GNSS pocket on the screen tub

**Phase:** Hardware tooling · **By:** Claude + operator

Operator decisions: the GNSS board stays with the screen (all its wiring there), on the back plate, raised if needed; the
6 mm thickness is the bare board. The carrier may grow for an even screw pattern, a visible logo, and nothing under the Wio.

- **Carrier v1.4** (`hardware/c5-wio-carrier/v1.4/`, from v1.3 by `make_v14.py`): 79.0 x 35.0 mm; four M2 holes on a
  72.8 x 29.0 rectangle (H4 new, board-only); logo and name in a bottom strip; C1/C2 above the Wio on its supply pins; test
  points in one column; two top-edge notches over the Cardputer's back-cover screws. See its README. v1.4 is now current.
- **Screen tub** (`enclosure/screen-gnss/gps_pocket.py`): D1 gets a 19.6 x 17.6 window in its back plate, in the middle band
  on the wire-entry side. A separate cap G1, shaped like a hat with its brim inside the tub, fills it and stands 3.2 mm proud.
  Measured on the D1 file: display board back at y = 4.0, back plate inner face at 7.4, so 3.4 mm free without the cap and
  6.6 mm with it.
- **Backpack** refitted to v1.4: four corner standoffs, no post or lips.

**Verified:** v1.4 ERC 0 violations; DRC (all severities, zone refill, schematic parity) 0 violations, 0 unconnected, 0 parity
issues. Tub, cap and GNSS board envelope do not overlap; tray, lid and board envelope do not overlap; all printable parts
slice in OrcaSlicer 2.5.0 without supports.

**Not verified:** nothing fabricated, printed or fitted; v1.4 routing not reviewed by eye in KiCad. Whether the coax reaches
from the hinge bar to the cap (about 60 mm of its 95 mm is used getting into the tub) is a paper estimate.

## 2026-11-15 — Carrier v1.3 (compact strip), measured antenna, shell refit

**Phase:** Hardware tooling · **By:** Claude + operator

Operator measurements (calipers, rounded up to the mm): GNSS antenna 18 x 6 x 6, GNSS board 16 x 14 x 6, XIAO 23 x 18 x 5.

- **Carrier v1.3** (`hardware/c5-wio-carrier/v1.3/`, from v1.2 by `make_v13.py`): the operator wanted the wasted space out.
  The top strip is removed, board 79.0 x 38.2 -> 79.0 x 21.7 mm; J1, F1, D1, test points, C1, C2 and the mounting holes
  moved beside and between the modules; 5 V, 3V3 and UART re-routed; LoRa routes untouched. See its README.
- **Hinge bar:** antenna size is now measured; the bar comes out 14.8 mm deep (was 16.8 with the guess, original 10.07).
- **Backpack:** refitted to v1.3. The board lies along the bottom wall; the upper half of the tray is empty apart from the SMA
  pigtails, and both Cardputer back screws stay reachable with the board in. USB end held by a post and two wall lips.

**Finding:** the GNSS board at 6 mm does not fit behind the display in the Cap TFT tub (about 3.7 mm free there). Its place is
an open decision.

**Verified:** v1.3 ERC 0 violations; DRC (all severities, zone refill, schematic parity) 0 violations, 0 unconnected, 0 parity
issues. Enclosure solids valid, no overlaps, all printable parts slice without supports.

**Not verified:** nothing fabricated, printed or fitted; v1.3 routing has not been reviewed by eye in KiCad. Wio height on the
carrier, USB-C plug height, SMA bulkhead dimensions and the Cardputer back-screw length are still guesses.

## 2026-11-15 — Carrier v1.2 (Grove to the right edge), enclosure draft 2

**Phase:** Hardware tooling · **By:** Claude + operator

**Carrier v1.2** (`hardware/c5-wio-carrier/v1.2/`, a copy of v1.1 changed by `make_v12.py`): the operator found the Grove
jack on the wrong side. Board widened from 58.0 to 79.0 mm so it spans the backpack shell; J1 moved to the right edge top
corner facing right; H1 moved from (55.0, 3.2) to (75.0, 15.5) under it; the three Grove nets re-routed on F.Cu along the
top strip (5 V at 0.5 mm, was 0.2 mm) to vias onto the existing B.Cu runs. Schematic unchanged apart from the revision
field. USB-C stays on the left edge. v1.1 is untouched.

**Enclosure draft 2** (`hardware/enclosure/`):

- Hinge bar: the antenna pod is gone. The bar is uniformly deeper (16.8 mm with the guessed antenna size, original 10.07)
  with the antenna inside the left half and the spare coax in the hollow right half; the original cover screw and hook stay.
- Backpack: redrawn for v1.2. Lid with a 2.6 mm edge chamfer, panel lines after the Cap TFT lid, debossed mark and name,
  two countersunk screws in the top corners and two tongues into the bottom wall; raised SMA seats at Y = +/-24; three fin
  grooves round the sides and bottom; Grove mouth in a scallop on the Cardputer's Grove side; USB-C on the other side with a
  seat for a TPU cap. 16.9 mm thick with both modules soldered flat.

**Verified:** v1.2: KiCad 10.0.7 ERC 0 violations; DRC with zone refill and schematic parity 0 violations, 0 unconnected,
0 parity issues (`v1.2/verification/`). Enclosure: all solids valid, no volume overlap between tray, lid and the board
envelope, nor between bar, cover and antenna envelope beyond the original pair's 32.7 mm3; every printable part slices in
OrcaSlicer 2.5.0 without supports.

**Not verified:** nothing fabricated, printed or fitted. v1.2 has no Gerbers and no physical review; the Grove footprint is
still the planning drawing v1.1 used. Antenna size, module height, USB-C plug height, SMA bulkhead dimensions and the
Cardputer back-screw length are still guesses (GUESS in the scripts).

## 2026-11-15 — Enclosure drafts: GNSS hinge bar and full-width backpack shell

**Phase:** Hardware tooling · **By:** Claude + operator

Started `hardware/enclosure/` (local-only, see its README). Two parametric FreeCAD drafts, built headless with `freecadcmd`:

- **`screen-gnss/`** — the Cap TFT V2 hinge bar (S1) and cover (C1) with a pod on the bar's outer face, beside the wire
  channel, for the ATGM336H kit's ceramic bar antenna. The original STLs are converted to solids and modified; the other
  five Cap TFT V2 parts are unchanged. The coax enters the existing wire channel through a notch. The GNSS board itself is
  meant to sit behind the display in the tub (about 3.7 mm free there).
- **`backpack/`** — an 84 x 54 mm tray, flat lid and four pegs for the v1.1 carrier on the Cardputer's back: two M2
  through the tray floor into the back-cover screw positions, pegs in the four 4.85 mm holes along the top edge, two SMA
  D-holes in the top wall 9 mm behind the back face (clear of the hinge bar and pod), USB-C and Grove openings in one side
  wall. 16.7 mm thick with both modules soldered flat.

Findings on the references: the Cap TFT V2 bar is about 8 x 10 mm in section and has no room inside for the antenna; the
Lab5 MonsterC5 tray pocket is about 56.5 mm wide (v1.1 is 58.0 mm) and the two-piece back shell is drawn for another board.

**Verified:** solids valid; no volume overlap between the new bar, cover and antenna envelope beyond the 32.7 mm3 the
original S1/C1 pair already has; no overlap between tray, lid and the board envelope; all five printable parts slice in
OrcaSlicer 2.5.0 without supports (generic 0.2 mm profile). Positions on the Cardputer come from M5Stack's published
`Cardputer-Adv.stl`.

**Not verified:** nothing printed or fitted. Antenna size, module height, USB-C plug height, SMA bulkhead hole and the
Cardputer back-screw length are guesses marked GUESS in the scripts. The carrier's Grove/USB-C edge ends up on the
audio-jack side of the Cardputer, opposite its Grove port: open question for the next carrier revision.

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
