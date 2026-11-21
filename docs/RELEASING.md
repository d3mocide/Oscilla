# Cutting a version

A version is a **milestone**: a known-good, buildable, checked tree with an
honest record of what is and isn't verified. It does not close phases. Phases
close only when their exit gate is demonstrated (AGENTS.md §6). A cut records
whatever phase status is true on that day.

## Numbering

`MAJOR.MINOR.PATCH` in the repo-root [`VERSION`](../VERSION) file. Both
firmwares read it: the probe's `PROJECT_VER` (reported by `version` and
`[HELLO]`) and the deck's `OSCILLA_DECK_VER` (wardrive CSV header).

- **Minor** for a milestone cut.
- **Patch** for a fix-only cut on top of a tag.
- **1.0.0** only when every v1 phase exit gate (P0–P8) is met on hardware.
- `OCP_PROTO_VERSION` is separate. Bump it only when the wire contract
  changes (DESIGN §5.7).

## Checklist

1. **Clean tree.** Commit everything in logical groups and stage by path,
   never `git add -A` (AGENTS.md §6). `git status` must be empty.
2. **Host gate:** `ASAN_OPTIONS=detect_leaks=0 ./tools/check_protocol.sh`
   and `python3 tools/ocp_repl.py --selftest`.
3. **Firmware gate:** `./tools/build_firmware.sh` and
   `./tools/build_firmware.sh --bench --probe-only`, with the toolchains it
   pins.
4. **UI gate:** `python3 tools/render_external_views.py --sync-guide-assets`
   leaves no diff. `check_ui_palette.py`, `check_rx_only.py` and
   `check_deck_stack.py` pass.
5. **Bump `VERSION`.** Add a `CHANGELOG.md` entry covering what was added,
   fixed and changed, plus **Known open** for every unmet gate.
   Operator-reported evidence is labelled as such.
6. **Update `ROADMAP.md`** (header, status board, phase notes) and add a
   `WORKLOG.md` entry for the cut.
7. **Flash both boards** from the release commit and confirm `[HELLO]`
   reports the new version. If that isn't done, say so in the WORKLOG entry.
8. **Tag:** `git tag -a vX.Y.Z -m "Oscilla vX.Y.Z"` on the release commit.
   Agents commit and tag; Will pushes (`git push && git push origin vX.Y.Z`).

Builds are not hardware evidence (BUILD_REPRODUCIBILITY.md). A green
checklist means the tree is consistent, not that the radios were
re-qualified.
