# 144 BPM / 400% — investigating a recurring swell

**The reported listening issue is still open.** No production code, output
settings or installed executable were changed. Both installed shortcuts point
to 0.4.0; its hash matches the tested build. No installed Pulse practice was
running at the initial check, so this is a controlled reproduction attempt with
the same app sources, not a recording of the user's affected session.

## Listening comparison

[20-second, lossless reference](reference-144.flac): 144 BPM, 4/4, Clean tone,
400% app gain, no subdivisions or second pulse. Start at a comfortable listening
volume. Play the file in a separate player using the same physical output. The
file contains the first 48 periods of the production offline render (48 kHz,
dual mono, 16-bit PCM encoded losslessly, no level normalization). Decoded the
FLAC and compared all 48 periods sample-for-sample; all are identical. Different
players can have different session levels or effects, so a comparison at a very
different listening level is not conclusive.

If the file also swells, the Pulse scheduler is not necessary for the effect.
Compare with another output, such as wired headphones connected directly to
the computer. If only live Pulse swells at a comparable listening level, capture
the actual affected session before changing its controls. No microphone or
system-wide recording was taken.

## Reproduction on Windows

- Played all four click sounds for 24 seconds **each**, in a hidden native
  Windows instance. Selected 144 BPM, 400% app gain, 4/4, one click per beat;
  no intentional silence or second pulse. Only the test session was attenuated
  to 15%. Windows master volume was not changed.
- 226 complete clicks: Clean 57, Woodblock 56, Bright 57, Full 56. Within each
  sound the digital waveforms matched sample-for-sample: 0 dB peak and energy
  spread, zero measured FFT difference. Measured interior intervals were all
  20,000 frames (416.67 ms). Capture-edge partial clicks do not contribute
  to the interior spread. FFmpeg separately checked whole-click RMS and peak.
- Sampled UI/requested gain stayed at 400; session and endpoint level/mute
  readbacks did not change. No discontinuity or timestamp error flag. Optional
  device positions unavailable. [Metrics](measurements.json) ·
  [Clean tone waveform, click overlay and spectrogram](clean-tone.png).
- The default multimedia endpoint was a Samsung display via NVIDIA HDMI;
  the engine mix format was stereo, 48 kHz. Windows' effects-discovery API
  returned no listed effects for the queried default/Other path. That is not
  proof that no effects are active: this capture does not measure display,
  soundbar, amplifier or acoustic output. The actual session/listening level
  may differ from the reduced-level test.
- On Samsung models with [Auto Volume / Sound Mode](https://www.samsung.com/us/support/answer/ANS10006961/),
  a reversible comparison is Auto Volume off / Standard sound, **if** the
  listener actually uses that display. No display settings were inspected or
  changed, and no specific device or feature is established as the cause.

## Repeat

Build the native audit and reference renderer with `bash scripts/test-evenness.sh`.
From Windows, run `scripts/test-evenness-windows.ps1 -SoundSweep -Bpm 144 -Gain 400 -OutputDirectory build/exact144`.
Analyze in Linux with `scripts/analyze-evenness.py build/exact144 --native --renderer build/evenness_audit --expect-flat`
and `scripts/verify-evenness-ffmpeg.py build/exact144 --expect-flat` using
the project audio Python environment. Use a fresh output directory.
