"""Known-signal checks for numbered-beat and long-run drift detection."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('four_four', Path(__file__).parents[1] / 'scripts/summarize-four-four.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class FourFourTests(unittest.TestCase):
    def case(self, gain=lambda i: 1, first=0):
        return {'rate': 48000, 'alignment_offset_frames': 9876, 'per_click': [
            {'frame': 9876 + i * 18000, 'energy': gain(i) ** 2, 'peak': gain(i)}
            for i in range(first, 1600)]}

    def test_identical(self):
        result, _ = audit.summarize(self.case(), 160)
        self.assertEqual(result['clicks'], 1600)
        self.assertEqual(result['between_beat_mean_spread_db'], 0)
        self.assertTrue(all(x == 0 for x in result['per_beat_minute_mean_spread_db'].values()))

    def test_first_beat_emphasis_detected(self):
        result, _ = audit.summarize(self.case(lambda i: 2 if i % 4 == 0 else 1), 160)
        self.assertAlmostEqual(result['between_beat_mean_spread_db'], 6.020599913, places=8)
        self.assertEqual(result['by_beat']['1']['mean_energy'], 4)

    def test_one_beat_gets_louder_after_five_minutes(self):
        result, _ = audit.summarize(self.case(lambda i: 1.1 if i >= 800 and i % 4 == 2 else 1), 160)
        self.assertGreater(result['per_beat_minute_mean_spread_db']['3'], .8)
        self.assertEqual(result['per_beat_minute_mean_spread_db']['1'], 0)

    def test_partial_first_click_does_not_renumber_beats(self):
        result, rows = audit.summarize(self.case(first=1), 160)
        self.assertEqual(rows[0]['beat'], 2)
        self.assertEqual(result['by_beat']['1']['clicks'], 399)
        self.assertEqual(result['by_beat']['2']['clicks'], 400)

    def test_missing_minute_rejected(self):
        case = self.case()
        case['per_click'] = case['per_click'][:1440]
        with self.assertRaises(ValueError):
            audit.summarize(case, 160)

    def test_bad_grid_rejected(self):
        case = self.case()
        case['per_click'][20]['frame'] += 3
        with self.assertRaises(ValueError):
            audit.summarize(case, 160)


if __name__ == '__main__':
    unittest.main()
