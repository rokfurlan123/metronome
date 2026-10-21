# Pulse

A native Windows desktop metronome for steady practice, changing meters, and layered rhythms. Built with C++20, Win32 controls, and WASAPI audio. No browser, .NET runtime, account, or network connection is required.

## Run

Pulse 0.3.0 opens as a compact **always-on-top overlay widget** on Windows 10/11 x64. Run `dist/Pulse.exe`, or extract `dist/Pulse-0.3.0-windows-x64.zip` and run `Pulse.exe`. This is an unsigned development build. Installed on this laptop at `%LOCALAPPDATA%\Programs\Pulse\0.3.0\Pulse.exe`, with Desktop **Pulse** and Start menu **Pulse Metronome** shortcuts.

### Overlay widget

- Drag the **Pulse** header to position the widget over your practice material.
- Enter BPM, use/hold **− / +**, or **Tap** a tempo. **Start / Stop** and the beat indicators use the existing audio-clock engine, including mixed-meter sequences and silent beats.
- **Editor** opens the complete rhythm/practice interface. **Widget** switches back without restarting playback or discarding settings. The editor placement and widget position are remembered for this session.
- **100% / 85% / 65%** cycles window opacity.
- **Lock** makes the widget click-through, letting you use the application underneath. Press **Ctrl+Alt+P** anywhere to unlock it. If Windows cannot register that shortcut (for example another Pulse instance owns it), Lock is disabled so you cannot accidentally strand the widget.
- **×** or Alt+F4 closes Pulse and stops audio without an unsaved-changes prompt. Preset saving remains manual.

The widget stays above ordinary windows; exclusive-fullscreen games and Windows secure screens are not supported overlay targets. Widget position, opacity and view choice are session-only, not preset settings. The unlock shortcut only restores mouse interaction; it does not start/stop playback globally.

For an optional per-user Windows installation and shortcuts, run `scripts/install-local.ps1` **only after authorizing Windows changes**. It installs version 0.3.0 under `%LOCALAPPDATA%\Programs\Pulse\0.3.0` and leaves older versions untouched. Building and packaging stay in Linux/WSL.

## Practice

- **Standard metronome:** set 20–300 BPM, choose a time signature, and press Start.
- **Mixed-meter sequence:** switch modes or choose **Try mixed meters**. Select each step to edit its meter, accent groups, repeat count, and optional tempo. Steps play in order and loop continuously.
- **Metronome boost:** in **Practice tools**, drag **Metronome volume (0–400%)** or enter a value. Changes apply to the next click during playback; 0% is silent, 100% is normal gain, 200% doubles gain, and 400% quadruples gain before peak limiting. A soft limiter reduces clipping on louder or overlapping clicks. Boost is saved with the preset. Windows volume remains a separate 0–100% control.
- **Full click — longer:** choose this under **Click sound** for a fuller tone with extra harmonics and a 100 ms tail (older sounds have 50 ms tails). Sound selection works during playback and is saved in presets. Combine it with the metronome boost when you need a stronger click.
- **Windows volume:** drag the slider at the top left/right to change the computer’s default output volume (affects all apps). It works during playback and follows external Windows volume changes. The current Windows mute state is preserved and shown. This setting is not stored in presets; Metronome volume in Practice tools remains a separate app setting.
- **Equal click sounds:** in **Practice tools**, uncheck **Accent beats**. Every click uses the same pitch and gain, including subdivisions and the second pulse. Coincident pulses become one click, and long tails fade before the next click to prevent overlap emphasis. Beat indicators also stop showing accent groups. This works during playback and is saved in presets. Older presets keep accents enabled.
- **Hold +/−:** one step immediately, then repeated steps after 400 ms, accelerating after 1.4 seconds. Release or move outside the button to stop. Works during playback, within 20–300 BPM.
- **Accent groups:** for 7/8, enter `2+2+3` or `3+2+2`. Values must add up to the beat count. Leave blank to accent only the first beat.
- **Repeats:** play each sequence step for 1–9999 bars, or check **Forever** to hold that meter indefinitely. During playback, **Next step** moves on at the next bar boundary. Standard metronome mode and the complete sequence already loop indefinitely. Step BPM `0` follows the main tempo; 20–300 sets a local tempo.
- **Add as new step:** insert a copy of the current editor values after the selected step. Apply changes replaces the current step. Moving to another step or mode commits valid editor values.
- **Subdivisions:** 1–4 clicks per notated beat, including triplets.
- **Second pulse:** evenly space 1–16 lower-pitched clicks over the whole bar. Use `0` to disable it. A 4/4 bar with a second pulse of 3 gives 3 against 4.
- **Silent beats:** mute a percentage of the main beats, including their subdivisions, while preserving timing and a gold visual pulse. The second pulse stays audible. The pseudorandom practice pattern restarts on each Start.
- **Sound:** clean tone, woodblock, or bright click; separate overall volume control.
- **Save / load:** use `.pulse` files to keep multiple practices. Presets include both standard settings and the sequence, including Forever. New files use format version 2; existing version 1 practices still load. No automatic settings files are written.

**Tempo always means quarter notes per minute.** At 120 BPM, a 4/4 bar takes 2 seconds, a 7/8 bar takes 1.75 seconds, and a 3/4 bar takes 1.5 seconds. Subdivisions divide the denominator beat, and accents do not change beat duration. A 6/8 bar therefore contains six eighth-note clicks; `3+3` gives two strong group accents.

In either view, press **Space** to start/stop, **T** to tap quarter notes, **Ctrl+S** to save, and **Ctrl+O** to load. Space and T are reserved for text entry while an edit field has focus. The BPM field, +/− buttons, and tap tempo stay available during playback. Clicking into the BPM field selects its number; Ctrl+A also works. Typing waits 250 ms for the complete value, while Enter, +/−, and tap tempo commit immediately. Valid changes apply at the next unrendered main beat without restarting or losing your place; subdivisions and the second pulse follow the new tempo. Explicit per-step BPM still overrides the main tempo. Meter and sequence editing remain locked until you stop. The compact window supports Windows DPI scaling. **Rhythm** and **Practice tools** are separate views; the main window has no scrolling canvas. Only a long sequence list scrolls.

## Build and check (Linux/WSL)

```bash
./scripts/test.sh
./scripts/build.sh
```

The build uses a locally extracted MinGW-w64 toolchain at `/home/rokdbest/tools/mingw/usr`. Set `PULSE_MINGW_ROOT` for a different location with the same directory structure. See [toolchain notes](docs/TOOLCHAIN.md).

The Linux tests exercise the same timeline and synthesizer used by the Windows application. AddressSanitizer and UndefinedBehaviorSanitizer are enabled. LeakSanitizer is disabled because it cannot run under the tracing environment used here.

The optional `tests/ui_smoke.cpp` driver builds the same app source into a test harness and exercises its native Windows controls. Earlier releases passed on real Windows. Version 0.3.0 has Linux Wine development-aware smoke coverage for the widget and editor, plus a native Windows installation/launch check. Physical click-through, global shortcut delivery and mixed-DPI movement still need native Windows validation. `scripts/test-windows.ps1` runs that harness on Windows; all compilation stays in Linux. `scripts/test-ui.sh` uses the local test runtime; pass `--hotkey-conflict` to reserve the unlock shortcut before launch and verify that Lock is safely disabled; this is a development check, not a dependency for Windows users.

## Design and limitations

Audio is synthesized at 48 kHz in an event-driven WASAPI shared-mode stream. Absolute fractional sample positions carry between bars, avoiding repeated rounding drift. The visual playhead follows the audio clock rather than driving the clicks. Playback uses the default Windows output device and reports audio failures; stop/start reconnects after a device change.

This first version has one optional second pulse, up to 128 sequence steps, and meters of 1–32 beats over /2, /4, /8, or /16. It does not yet include MIDI sync, gradual tempo ramps, voice counting, or an installer. Bluetooth and hardware output latency depend on the device. Real Windows rendering and audio startup have been checked. Audio listening quality has not been assessed; the automated playback test uses zero volume.

Reference: [Time Guru by Avi Bortnick](https://www.avibortnick.com/timeguru.html), especially mixed-meter practice, tempo programs, and silent-beat exercises. Pulse is an independent implementation with its own interface and synthesized sounds.

Closing Pulse exits immediately without an unsaved-changes prompt. Use Save before closing if you want to keep your practice settings.
