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

## 2026-10-31 — dense irregular grids and new startup defaults

Fresh launches now use 200% gain with accents disabled. Explicit file-preset settings are preserved, including legacy formats 1–2 retaining their old accent behavior.

A further reproducible edge case remained: 300 BPM, 7/16 grouped 2+2+3, four subdivisions, 13 second pulses/bar, 400% gain. The previous per-onset tail truncation produced **0.512 / 0.965 / 0.637 / 2.479 dB** energy spreads across clean/woodblock/bright/full sounds. This is a dense overlapping-grid case, **not proof of the cause of the user's ordinary metronome listening report**.

Unaccented synthesis now uses the shortest grid spacing across the active practice as a shared envelope limit (`bar duration / lcm(main subdivisions per bar, second pulses per bar)`). Every onset has the same tail length, pitch and gain; coincident pulses still form one voice. The limit is floored to whole samples for conservative fractional scheduling, and live main-BPM changes update the render settings at the same beat boundary. Accent-enabled synthesis remains unchanged. Extremely dense grids consequently shorten all click tails consistently rather than varying them from hit to hit.

The expanded audit renders **1,080** complete-stream cases, including **36 new dense cases at 200/300/400% gain**. All **1,068 unaccented cases** have **0 dB energy spread and zero onset-aligned sample difference**. Twelve positive accent controls remain measurably accented (at least 0.5 dB). [Current audit CSV](design-review/audio-audit.csv). The existing sanitizer suite passes **272,573 checks**.

These are digital-render measurements. The installed Windows app has not been updated, and no new physical listening or Windows-loopback capture has been performed. Hardware processing, downstream gain and the originally reported simple setup remain listening-validation limits.
