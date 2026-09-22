---
name: oscilla-kicad
description: Create, review, verify, or release KiCad schematic and PCB artifacts for Oscilla hardware. Use for the C5 and single-Wio carrier, KiCad ERC/DRC, renders, or fabrication exports; do not use for firmware-only changes.
---

Create evidence-backed KiCad work for Oscilla. A clean file or CLI check is not a hardware qualification.

## Start here

1. Call `kicad_status` before an automated KiCad operation. Stop if KiCad is unavailable or its version lacks the required command.
2. Treat the project-root `AGENTS.md`, `Research/c5-backpack-design.md` Rev D, and `docs/DECISIONS.md` as authority. Read the applicable source rather than recreating pin assignments from memory.
3. Read [the single-Wio carrier reference](../../references/single-wio-carrier.md) when the task involves that carrier. Read [release gates](../../references/release-gates.md) before fabrication output or an order recommendation.

## Work modes

- **Schematic:** establish all explicit power, ground, SPI/control, pull, and bypass nets. Run ERC and inspect each reported error or warning; do not suppress a finding without a design rationale.
- **PCB:** place the local 3V3 bypass components near the Wio connector, use a continuous ground reference, keep SPI/control routes short, and preserve test access. Run DRC with schematic parity after layout changes.
- **Review:** request board renders and inspect the physical result, including mounting, connector access, silkscreen polarity, antenna clearance, and component clearances. DRC does not establish any of these.
- **Release:** export fabrication data only after ERC/DRC and manual review pass. Inspect Gerber/drill output in a viewer before describing it as ready to order.

## Safety and evidence

- Do not silently change an electrical net, pull value, power source, antenna treatment, or board outline to make a check pass. Surface the discrepancy.
- Do not claim RF performance, power headroom, cold-boot reliability, USB recovery, or assembly fit from ERC/DRC/render output. These remain physical gates.
- Treat exported Gerbers, drill data, PDFs, and reports as build artifacts. Keep them out of source control unless the project release process explicitly tracks them.
