# Accent investigation and fix — Pulse 0.2.8

Completed 2026-10-15. Development-aware testing of the app's actual synthesis and Windows playback path.

## Findings

With accents disabled, the old synthesizer assigned equal pitch/gain to each individual click but still mixed simultaneous and overlapping voices. A main beat coinciding with the second pulse therefore made a stronger hit. Long click tails could also interfere at dense subdivisions. The visual beat strip continued to mark accent groups even when sound accents were disabled.

The reported ordinary 160 BPM setup (clean tone, no second pulse) produced uniform clicks **before and after** this change. This audit does not establish that overlap explained everything the user heard in that setup.

## Fix

Unaccented playback now uses one voice: coincident pulses produce one click. A click's tail is bounded by the next audible onset, with a short fade to zero when shortened; long tails cannot add to the next click. Changing from accented to unaccented playback clears residual accented tails at the next click. Accent-enabled mixing remains unchanged. With accents off, the visual beat strip no longer marks accent groups and explicitly says “Accents off — equal clicks.”

## Validation

- **272,558 ASan/UBSan checks passed**, including exact equality of single/coincident clicks across all four sounds and three gain levels, shortened-tail fade ending at zero, live accent-transition residuals, lookahead across silent/coincident events and bar boundaries, boost limiting, presets and rhythm timing.
- **1,044 complete-stream render cases passed**: all four sounds, 70/100/200/400% volume, 60/137/160/199/300 BPM, /4 /8 /16, subdivisions 1–4, 2+2+3 grouping, mixed meters, positive accent controls and 3-against-4 overlays. Additional boundaries cover 20/300 BPM, /2 /16, 1-beat and 32-beat meters. Representative cases also run at 44.1/48/96 kHz.
- Worst unaccented per-click energy spread in the initial matrix was **0.237536 dB** (full click, 199 BPM, /16, four subdivisions). After correction, the expanded matrix's worst spread is **0.001907 dB**, below the 0.01 dB regression limit. Residual sample-level differences come from fractional intervals rounded to whole samples and the corresponding final fade length.
- **Real Windows WASAPI loopback captures**, 48 kHz stereo, using the UI and actual audio worker, completed before and after the fix with **zero discontinuities and zero position gaps**. Only the audit process's session volume was reduced to 15%; Windows master volume was not changed. No microphone was used.

| Captured scenario at 160 BPM | Before: energy spread | After: energy spread |
| --- | ---: | ---: |
| Clean tone, 200%, live switch to accents off | 0.000000 dB | 0.000000 dB |
| Each of the four sounds, 400%, accents off | 0.000000 dB | 0.000000 dB |
| Full click, 400%, second pulse 3/bar, accents off | **3.695578 dB** | **0.000000 dB** |
| Full click, 400%, four subdivisions, accents off | 0.000000 dB | 0.000000 dB |
| Accents ON control, clean 200% | 3.663478 dB | 3.663478 dB |
| Accents restored ON, full click 400% | Not captured | 2.541749 dB |

The after-capture analysis passes level variation ≤0.05 dB, onset-aligned waveform correlation ≥0.999, regular onset spacing within one sample, and positive controls ≥0.5 dB. Regular recorded onsets are 375 ms apart; subdivisions are 93.75 ms apart. The second-pulse rhythm intentionally has unequal intervals. An early analyzer used tied envelope peaks on a limited waveform and falsely reported half-cycle jitter; the final analyzer aligns on the leading edge instead and applies the same method to both recordings.

The existing Windows native UI regression suite also passed, including playback controls and closing without an unsaved-changes dialog. Linux MinGW build is warning-clean.

## Evidence and repeatability

- [Offline before CSV](accent-audit-before.csv), [offline after CSV](accent-audit-after.csv).
- [Windows before metrics](windows-audio-before.analysis.json), [Windows after metrics](windows-audio-after.analysis.json).
- Raw float captures and format/segment metadata: `build/windows-audio-before.*` and `build/windows-audio-after.*` (kept locally).
- Portable suite: `bash scripts/test.sh`; complete-stream sweep: `bash scripts/test-accent-audit.sh`.
- Windows capture harness: `tests/windows_audio_audit.cpp`, using the existing Linux MinGW native harness build pattern; execute via `scripts/test-windows.ps1 -TestName windows_audio_audit`.
- Capture analysis: `/home/rokdbest/tools/dutch-audio-venv/bin/python scripts/analyze-audio-audit.py build/windows-audio-after --expect-flat` (existing NumPy 2.5.3 environment; no new dependency installed).

## Limits

These are digital-render and Windows-loopback measurements, not a microphone measurement of speakers/headphones or an independent listening assessment. The Windows test uses reduced test-session gain; it does not prove behavior of downstream hardware processing at every listening volume. Deliberate silent beats, explicit tempo changes and polyrhythm intervals still affect perceived rhythm. No claim is made that the previously reported plain 160 BPM case had measured audio accents.
