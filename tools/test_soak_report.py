"""Synthetic failures for the count-only endurance report."""
import unittest
from soak_report import summarize


def sample(t, free=120000, **changes):
    return dict(elapsed_s=t, phase='steady_long', deck_free=free,
                probe_free=94788, deck_largest=96000, probe_largest=65536,
                stack_free_min=2768, deck_uptime_s=t, probe_uptime_ms=t*1000,
                **changes)


class ReportTest(unittest.TestCase):
    def test_warmup_and_lifetime_minimum_are_not_leaks(self):
        rows = [sample(0, 180000)] + [sample(t, deck_min=50000)
                                      for t in range(600, 1801, 30)]
        report = summarize(rows)
        self.assertEqual(report['steady_heap']['deck_free']['slope_bytes_min'], 0)
        self.assertEqual(report['outcome'], 'running')

    def test_slow_loss_visible_after_warmup(self):
        rows = [sample(t, 120000-t) for t in range(0, 1801, 30)]
        self.assertEqual(summarize(rows)['steady_heap']['deck_free']['slope_bytes_min'], -60)

    def test_failed_run_cannot_be_completed_by_duration(self):
        rows = [sample(0), sample(7201), {'outcome': 'fault'}]
        report = summarize(rows)
        self.assertEqual(report['outcome'], 'fault')
        self.assertEqual(report['completed_stages'], [])

    def test_modes_and_closed_heap_do_not_pollute_steady_fit(self):
        rows = [sample(0), sample(600), sample(1200)]
        rows.append(dict(sample(1500, 180000), phase='finished'))
        self.assertEqual(summarize(rows)['steady_heap']['deck_free']['slope_bytes_min'], 0)


if __name__ == '__main__':
    unittest.main()
