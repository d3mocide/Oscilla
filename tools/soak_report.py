"""Summarize count-only endurance JSONL without opening a serial port."""
import argparse
import json
import statistics
from datetime import datetime, timezone
from pathlib import Path


def summarize(records):
    rows = [r for r in records if 'deck_free' in r and 'phase' in r]
    steady = [r for r in rows if r['phase'] == 'steady_long']
    warmed = ([r for r in steady if r['elapsed_s'] >= steady[0]['elapsed_s']+600]
              if steady else [])
    fits = {}
    for key in ('deck_free', 'probe_free', 'deck_largest', 'probe_largest'):
        if len(warmed) < 2:
            continue
        xs = [r['elapsed_s']/60 for r in warmed]
        ys = [r[key] for r in warmed]
        xm, ym = statistics.mean(xs), statistics.mean(ys)
        denominator = sum((x-xm)**2 for x in xs)
        if denominator:
            fits[key] = dict(first=ys[0], last=ys[-1], minimum=min(ys),
                             maximum=max(ys), slope_bytes_min=round(
                                 sum((x-xm)*(y-ym) for x, y in zip(xs, ys))/denominator, 3))
    return dict(
        updated_utc=datetime.now(timezone.utc).isoformat(),
        outcome=next((r['outcome'] for r in reversed(records) if 'outcome' in r), 'running'),
        last_event=next((r for r in reversed(records) if 'phase' not in r), None),
        completed_stages=[r['stage_complete'] for r in records if 'stage_complete' in r],
        samples=len(rows), latest=rows[-1] if rows else None,
        steady_sample_span_s=round(steady[-1]['elapsed_s']-steady[0]['elapsed_s'], 2) if steady else 0,
        warmup_excluded_s=600, steady_heap=fits,
        minimum_stack_bytes=min((r['stack_free_min'] for r in rows), default=None),
        interpretation='Heap trends are observations, not automatic leak or qualification verdicts.')


def write_report(records, path):
    path = Path(path)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(summarize(records), indent=2)+'\n')
    temporary.replace(path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    records = [json.loads(line) for line in args.input.read_text().splitlines() if line.strip()]
    if args.output:
        write_report(records, args.output)
    else:
        print(json.dumps(summarize(records), indent=2))
