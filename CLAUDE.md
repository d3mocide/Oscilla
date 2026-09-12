# CLAUDE.md

**The agent instructions for this repository live in [`AGENTS.md`](AGENTS.md).
Read that file.** It is tool-neutral on purpose, so every coding agent works
from the same rules and there is only one copy to keep true.

Nothing Claude-specific is needed yet. If something ever is — a Claude Code
hook, a skill, a permission note — it goes *here*, and everything else stays in
`AGENTS.md`. Do not copy `AGENTS.md` content into this file.

Quick orientation while you open it:

- **Receive-only on every radio.** No transmit verb is compiled into any build.
  If a task seems to need one, stop and ask.
- **Hardware authority is `Research/c5-backpack-design.md` (Rev D).** Never
  write a pin constant from anywhere else.
- **Verify before claiming.** `./tools/check_protocol.sh`,
  `python3 tools/ocp_repl.py --selftest`, `./tools/build_firmware.sh`.
