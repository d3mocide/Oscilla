#!/usr/bin/env python3
"""Check the shared fixed-size RF-event backpressure boundary."""
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def main(root: Path = ROOT) -> int:
    frame = root / "firmware-c5/main/ocp_frame.c"
    if not frame.exists():
        print("FAIL: shared OCP event emitter is missing")
        return 1
    code = frame.read_text(errors="replace")
    problems = []
    required = (
        ("xQueueCreate(OCP_EVENT_QUEUE_DEPTH, sizeof(ocp_event_record_t))", "queue is not sized from OCP_EVENT_QUEUE_DEPTH"),
        ("xQueueSend(s_event_queue, event, 0)", "event submission may block a radio callback"),
        ("xQueueReceive(s_event_queue, &event, portMAX_DELAY)", "one task does not own event formatting"),
        ("s_event_dropped[event->kind]", "full queues are not counted by event kind"),
        ("bool ocp_event_submit", "typed event submission boundary is missing"),
    )
    for needle, message in required:
        if needle not in code:
            problems.append(message)

    for path in sorted((root / "firmware-c5/main").glob("*.c")):
        if "ocp_emit_event(" in path.read_text(errors="replace"):
            problems.append(f"{path.relative_to(root)} still formats events in a producer")

    if problems:
        print("FAIL: shared event queue invariant:")
        print("\n".join(f"  {p}" for p in problems))
        return 1
    print("  event queue: fixed-size nonblocking producer boundary verified")
    return 0


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1]) if len(sys.argv) == 2 else ROOT))
