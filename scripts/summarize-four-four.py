"""Compare numbered beats and minute-to-minute levels in a native 4/4 audit."""
import argparse
import csv
import json
import math
from pathlib import Path


def summarize(case, bpm, seconds=600):
    period = case['rate'] * 60 / bpm
    offset = case['alignment_offset_frames']
    rows = []
    for click in case['per_click']:
        position = (click['frame'] - offset) / period
        ordinal = round(position)
        if abs(position - ordinal) * period > 1.001:
            raise ValueError('Click does not match the fixed sample-clock grid')
        time = ordinal * 60 / bpm
        if not 0 <= time < seconds:
            continue
        rows.append(dict(click, seconds=time, bar=ordinal // 4 + 1,
                         beat=ordinal % 4 + 1, minute=int(time // 60) + 1))

    def group(part):
        if not part:
            raise ValueError('Missing numbered beat/minute coverage')
        energies = [r['energy'] for r in part]
        if not all(math.isfinite(e) and e > 0 for e in energies):
            raise ValueError('Invalid click energy')
        return {'clicks': len(part), 'mean_energy': math.fsum(energies) / len(part),
                'energy_spread_db': 10 * math.log10(max(energies) / min(energies)),
                'maximum_peak': max(r['peak'] for r in part)}

    beats = {str(b): group([r for r in rows if r['beat'] == b]) for b in range(1, 5)}
    minutes = {str(m): {str(b): group([r for r in rows if r['beat'] == b and r['minute'] == m])
                       for b in range(1, 5)} for m in range(1, math.ceil(seconds / 60) + 1)}
    means = [g['mean_energy'] for g in beats.values()]
    drift = {}
    for b in beats:
        levels = [minute[b]['mean_energy'] for minute in minutes.values()]
        drift[b] = 10 * math.log10(max(levels) / min(levels))
    return {'analyzed_seconds': seconds, 'clicks': len(rows), 'by_beat': beats,
            'by_minute': minutes, 'between_beat_mean_spread_db': 10 * math.log10(max(means) / min(means)),
            'per_beat_minute_mean_spread_db': drift}, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    captures = {r['case']: r for r in csv.DictReader((args.folder / 'capture.csv').open())}
    analysis = json.loads((args.folder / 'analysis.json').read_text())
    # Preserve failed runs and still produce their numbered-beat diagnostics.
    failures = list(analysis['failures'])
    case = next(c for c in analysis['cases'] if c['case'] == 'ten_minute_4_4')
    capture = captures[case['case']]
    if any(int(capture[k]) != v for k, v in {'beats': 4, 'unit': 4, 'subdivision': 1, 'poly': 0, 'accents': 0}.items()):
        raise ValueError('Expected plain unaccented 4/4')
    if int(capture['actual_playback_ms']) < 600000 or int(capture['frames']) / case['rate'] < 600:
        raise ValueError('Native capture did not run for ten real-time minutes')
    summary, rows = summarize(case, int(capture['bpm']))
    summary.update(settings=capture, capture_seconds=int(capture['frames']) / case['rate'],
                   leading_partial_clicks_excluded=case['leading_capture_partial_clicks_excluded'])
    control = next(c for c in analysis['cases'] if c['case'] == 'positive_control')
    if control['energy_spread_db'] < .5:
        raise ValueError('Known accented positive control was not detected')
    summary['positive_control_energy_spread_db'] = control['energy_spread_db']
    if summary['between_beat_mean_spread_db'] > .05 or max(summary['per_beat_minute_mean_spread_db'].values()) > .05:
        failures.append('Numbered-beat or time-dependent emphasis detected')
    summary['failures'] = failures
    with (args.folder / 'numbered-beats.csv').open('w') as out:
        writer = csv.DictWriter(out, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.folder / 'numbered-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({k: v for k, v in summary.items() if k != 'by_minute'}, indent=2))
    if failures:
        raise SystemExit('FAIL: ' + '; '.join(failures))
    print('PASS: all four numbered beats agree across ten real-time minutes')


if __name__ == '__main__':
    main()
