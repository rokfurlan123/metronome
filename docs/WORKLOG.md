# Pulse work log

## 2026-10-08 — First native Windows build

User request: create a new native Windows desktop metronome, including combinations of time signatures, with Metronome Guru as a possible reference. Research identified Time Guru by Avi Bortnick: https://www.avibortnick.com/timeguru.html. No response to the optional distinction between sequential mixed meters and simultaneous rhythms arrived during implementation, so this version includes both sequences and one optional second pulse.

### Delivered

- New isolated project at `/home/rokdbest/metronome` with a portable x64 Windows executable at `dist/Pulse.exe`.
- C++20 / Win32 interface, dark theme, standard keyboard navigation, per-monitor DPI handling, small-window scrolling, and visual beat/accent indicators.
- Standard metronome, 20–300 quarter-note BPM, tap tempo, meters through 32 beats with /2 /4 /8 /16 denominators.
- Mixed-meter sequences of up to 128 steps, 1–99 repeats per step, optional individual tempos, insert/remove/reorder, grouping such as 2+2+3.
- 1–4 subdivisions per denominator beat, one optional 1–16 pulse overlay per bar, randomized main-beat silence, three synthesized click timbres, and volume.
- Validated `.pulse` preset save/load, atomic replacement, unsaved-change prompts, and three example practices.
- Audio sample timeline preserves fractional timing between bars. Event-driven WASAPI audio runs independently of redraws, using the default Windows output endpoint. Device failure restores the controls and reports an actionable error.
- Build/test scripts, README, and local toolchain details. No Windows-drive files were modified, and no Windows toolchain was installed.

### Validation

- Linux core test suite: **17,252 assertions passed**, under AddressSanitizer and UndefinedBehaviorSanitizer. Covers standard timing, 4/4 → 7/8 → 3/4 transitions, accents, repeats, step tempos, subdivisions, 3:4 timing, silence, drift across 10,000 bars, preset round-trip/rejection, bounded audio synthesis, and click-tail cleanup.
- LeakSanitizer is disabled because the execution environment uses tracing and LSan cannot run under ptrace; ASan and UBSan remain enabled.
- Windows cross-build passed with `-Wall -Wextra -Wpedantic -Werror`. PE inspection confirms a native x64 GUI executable, importing only Windows system DLLs; C++ and POSIX support are statically linked.
- Native control smoke tests under Wine passed: launch/default values, switching modes, selecting 7/8, applying accent changes, add/move/remove, invalid beat rejection, audio-failure recovery, small-window scrollbars, unsaved-close protection, and clean shutdown.
- The initial smoke driver launched the actual portable executable; later scrollbar inspection moved into a same-process test build of the identical app source because Wine returned zero range information across processes. The resize test then passed (range 979, page 616).
- Wine cannot provide a usable audio endpoint in this environment. Error handling was exercised, but successful WASAPI playback, listening quality, and real Windows DPI behavior have **not** been verified.
- A parent-window GDI capture was inspected for layout, but Wine's capture omitted child controls; it is not a complete UI acceptance screenshot.

### Decisions and remaining scope

- Quarter-note BPM remains fixed across denominator changes; subdivisions split the notated denominator beat. This is documented with timing examples.
- Editing is disabled during playback, so changes apply cleanly at restart.
- The silent-beat pseudorandom pattern repeats after restarting; the optional second pulse is unaffected by muting.
- Presets are explicit files; there is no hidden auto-save or network use.
- First development build is unsigned and has no installer. MIDI sync, voice counting, gradual tempo ramps, and multiple simultaneous rhythm layers are outside this first version.
- Next acceptance step: open `dist/Pulse.exe` on Windows, listen on the intended audio device, and check display scaling. Native common file dialogs also need a Windows end-to-end check; the underlying preset format and validation have passed automated tests.

## 2026-10-08 — Windows desktop shortcut

At the user's explicit request, created `C:\Users\rocky\Desktop\Pulse.lnk`. Windows did not retain the direct UNC executable target in the initial shortcut, so the verified shortcut uses `C:\Windows\explorer.exe` with the quoted argument `\\wsl.localhost\Ubuntu\home\rokdbest\metronome\dist\Pulse.exe`. Verified the saved launcher, argument, and existence of the executable through Windows PowerShell. No application binary was copied to Windows. Shortcut creation and configuration are verified; the app was not launched during this task.

## 2026-10-09 — Saved session organized in Projects

Registered the project as **Metronome Pulse** in the active Codex Workspace Projects sidebar and base preferences. Attached the existing **Create Windows metronome app** conversation (`01a11bf2-c823-7d91-9636-d383b9de3109`). Source remains at `/home/rokdbest/metronome`; conversation history and original working folder are preserved. Verified project-name search and refreshed the sidebar.

## 2026-10-10 — Pulse 0.2.0: compact interface, Forever, local Windows launch

User reported continuing rendering artifacts, poor UI/UX, the need for unlimited repeats, and repeated unknown-source confirmations. This supersedes the earlier attempt to repair a scrolling full-window layout.

- Replaced the 1070×980 scrolling form with a 900×610 logical-pixel layout. A persistent tempo/transport/beat display sits above separate Rhythm and Practice tools views. Sequence editing appears only in Sequence mode. Native controls use a consistent light palette and Windows composition buffers parent/child painting together. Removed the parent scrollbars, canvas translation, and child-scroll repositioning path entirely.
- Added a visible Forever checkbox for each sequence step (stored as `repeats=0`), finite repeats through 9999, and Next step during playback. A queued advance takes effect at the next bar boundary and does not double-skip when a finite step was already due to end. Infinite steps do not grow a counter and cannot overflow it. Standard mode and whole-sequence looping remain indefinite.
- Presets now write format version 2; version 1 practices still load. Added core tests for 12,000 indefinite bars, exact timing, manual advance, last-finite-bar advance, wraparound, and persistence.
- Build version and manifest updated to 0.2.0. Builds atomically replace the Linux artifact instead of truncating a running image (introduced during the earlier scrolling fix).
- Inspected the actual Windows shortcut: it used Explorer to launch `\\wsl.localhost\Ubuntu\home\rokdbest\metronome\dist\Pulse.exe`. The user did not answer the optional warning-type clarification; proceeded with the verified network launch as the likely source.
- Installed the release at `C:\Users\rocky\AppData\Local\Programs\Pulse\0.2.0\Pulse.exe`, verified its SHA-256 against the Linux build, removed any download-zone marker only from Pulse and its shortcut, and changed `C:\Users\rocky\Desktop\Pulse.lnk` to target the local executable directly. No global Windows security settings, certificate trust, UAC, SmartScreen, or zone policies were changed. Build toolchains remain in Linux.
- Opened the updated shortcut through Windows shell execution and verified the installed Pulse process acquired a main window immediately, without waiting for a confirmation. Left the newly opened local app available to the user; existing user windows were not forcibly closed.

Validation:

- Core tests: **41,258 checks passed** with ASan and UBSan (LSan remains disabled for the traced environment).
- Warning-clean native x64 build passed.
- Ran the same-source native UI harness on **real Windows**, not just Wine. Passed launch, defaults, editing, adding/reordering/removing, audio startup at zero volume, stopping, invalid-input handling, Forever retention, eight repeated tab round trips, no parent scrolling, primary-control bounds, unsaved-close protection, and clean shutdown. Evidence: `build/windows-ui.log`.
- Captured and visually inspected real Windows screenshots of both views (`docs/preview.png`, `docs/practice-tools.png`); controls and text are present, fitting, and non-overlapping. Automated playback was muted, so subjective listening quality was not assessed.
- Installed executable SHA-256: `d57866f040110d554cbec3f20c2fda5db55327e791cc8cbfc4bc26ed2a38cf56`.

Implementation references: Microsoft [extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) for child-window composition, and [Attachment Manager](https://support.microsoft.com/en-us/windows/security/information-about-the-attachment-manager-in-microsoft-windows) for file-zone warnings. Local installation and file-specific unblocking address the observed launch arrangement; the app has not been publisher-signed.

## 2026-10-10 — Pulse 0.2.1: live tempo controls

User could not change tempo during playback. Enabled the BPM edit field, +/− buttons, Tap tempo button, and T hotkey during playback. Meter/sequence edits remain locked. Invalid partial text leaves the audio running at the last valid BPM; invalid text is restored on focus loss.

The audio thread receives tempo changes through an atomic value and applies them at the next unrendered main beat. Timeline events now retain their musical beat positions. Retiming preserves the exact fractional-sample boundary, the current bar/beat, accents, mute decisions, subdivisions, and polyrhythm alignment, and adjusts the next bar start without restarting the stream. Steps with explicit BPM retain their programmed tempo; the main BPM change applies to following steps that inherit the main tempo. The UI explains that case.

Validation:

- Core suite: **43,296 checks passed** under ASan/UBSan, including live slowdown/speedup within a bar, remaining subdivisions/polyrhythm positions, mixed-meter inheritance and step overrides, and 1,000 alternating live changes with no accumulated sample-rounding drift.
- Warning-clean Windows x64 build passed.
- Real Windows UI harness passed live typing, +/−, tap tempo, invalid-value restoration, playback continuation, and the existing native control/Forever/layout tests. Automated playback was muted. Log: `build/windows-ui.log`.
- Installed at `C:\Users\rocky\AppData\Local\Programs\Pulse\0.2.1\Pulse.exe` and updated the existing desktop shortcut. Verified installed binary hash and absence of a download-zone marker. Existing practice windows were not closed or restarted; reopen via the shortcut to use this version.
- SHA-256: `bb8c661ba481dbd56b3d46db287da785aec332f51467a6c35e8e1f19bf815d0f`.

## 2026-10-10 — Pulse 0.2.2: reproduced and fixed actual keyboard entry

User reported that live tempo still did not work. Inspected Windows: the desktop shortcut correctly targeted 0.2.1, with no Pulse process running at inspection time. A new regression test using actual SendInput mouse/keyboard events and measuring audio-clock beat notifications reproduced the failure: Ctrl+A did not select all in the native edit control. Typing 240 into 120 yielded `122400`; validation rejected it while the click stayed at 120 BPM. The previous UI test used WM_SETTEXT and therefore missed this interaction defect.

Changes:

- Explicit Ctrl+A handling for edit controls.
- Selecting the BPM field by focus selects its contents after mouse caret placement.
- A 250 ms typing debounce avoids applying intermediate values such as 24 while entering 240. Enter, focus loss, +/− and tap tempo commit immediately.
- Visible version number in the window title to distinguish older running windows.
- Added `tests/live_tempo_test.cpp` and parameterized the Windows test runner. This test stops input if its own window loses foreground focus.

Evidence:

- Before fix: `docs/live-tempo-before.txt` — requested 240, field contained 122400, measured beat interval 0.499897 s.
- After fix: `docs/live-tempo-after.txt` — field contained 240, measured 0.249988 s; field contained 60, measured 1.00007 s; + button to 61 measured 0.986883 s (expected 0.983607 s).
- The test uses a click into the BPM field without Ctrl+A for the first replacement, and actual Ctrl+A for the next replacement. Audio startup, timing changes and continuation are observed through the real WASAPI audio-clock notifications; test sound is muted.
- Existing Windows UI regression tests passed after the new interaction test. Warning-clean Windows build passed. Rhythm/audio algorithms were unchanged in this patch.
- Installed binary and desktop shortcut now target `%LOCALAPPDATA%\Programs\Pulse\0.2.2\Pulse.exe`. SHA-256 verified: `de01961b1d92dbe7e55605273075a172a651f1b6ffa3a1064280a9d3cf9d518d`.

## 2026-10-12 — Pulse 0.2.3: equal click sounds and held tempo controls

- Added Practice tools → Accent beats, enabled by default. Disable during playback or while stopped for identical pitch and gain on all main beats, subdivisions and second-pulse clicks. Rhythm/group timing, visual grouping and intentional silent beats remain intact. Live changes reach the synthesizer through an atomic setting.
- PULSE 3 presets preserve the accent option; PULSE 1/2 presets still load with accents enabled.
- Tempo +/− now step on mouse-down, repeat after 400 ms, then accelerate from 100 ms to 30 ms intervals after 1.4 seconds. Release, pointer exit, capture/focus loss and disable stop repetition. Native keyboard and programmatic button clicks still work. Tempo stays within 20–300.
- Validation: 65,802 core checks passed under ASan/UBSan, including sample-for-sample equality across click types for all three sounds and preset compatibility. Warning-clean MinGW Windows build passed. Real SendInput/WASAPI test passed live typing, single clicks, held +/−, acceleration, release/drag cancellation, bounds, accent checkbox during playback, and held input while stopped. Held + rose 120→179 in three seconds; release stayed stable. Development-aware tests, not independent testing. Log: build/windows-live_tempo_test.log.
- Built dist/Pulse.exe and dist/Pulse-0.2.3-windows-x64.zip.
- Remaining Windows UI regression suite passed after adding a synchronization delay to the harness selection helper (the initial run checked enabled state before the GUI completed its update). Inspected the Practice tools screenshot: Accent beats is visible and fits the layout.
- Added a custom teal/gold metronome icon embedded at seven resolutions (16–256 pixels), used by the native window and shortcuts. Reproducible icon generator: scripts/create-icon.py; existing Linux Pillow environment at /home/rokdbest/score-reader/.venv/bin/python, no new toolchain installed.
- Per user request, installation now creates a per-user Start menu shortcut named Pulse Metronome in addition to the desktop shortcut. Both target the local 0.2.3 executable and its embedded icon.
- Installed and opened 0.2.3 on Practice tools. Verified the Accent beats checkbox is visible, the executable icon is present, and Get-StartApps lists Pulse Metronome pointing to the new executable. Existing older windows were left untouched. SHA-256: 3b08cb5677b857abc16ea2b98e6b7d5178cc250f262cef7fe05c796fa57ecff8. Updated portable archive includes the final icon build.

## 2026-10-12 — Pulse 0.2.4: Windows output volume slider

- Added an always-visible native trackbar at the top of both tabs, controlling the default Windows multimedia output through IAudioEndpointVolume. Live dragging works during playback. Label shows percentage and Windows mute state; dragging preserves mute. The Practice tools volume is now labelled Metronome volume to distinguish it. System volume is never loaded/saved with presets.
- Initializes COM on the GUI thread; endpoint operations release all COM references. Re-resolves the default output on each operation and polls every 500 ms to reflect external volume/mute or output-device changes. Disables slider when no endpoint is available; failed writes show a status message. No changes to rhythm/audio scheduling.
- Warning-clean Linux MinGW build passed. New native Windows system_volume_test passed real SendInput dragging during WASAPI playback, endpoint readback, initial state, external change synchronization, mute preservation and visibility across tabs. Test restored the original Windows volume. Log: build/windows-system_volume_test.log. No hardware disconnect test was performed.
- Existing Windows UI regression suite passed. Inspected the updated Practice tools screenshot: global slider and label fit the top header; app volume, accents and existing controls remain visible.
- Installed 0.2.4 locally and updated both shortcuts. Binary hash verified: fd37bfb74872391739a14367156d9a5788668eafb15d0e54ade935473001282b. Portable archive: dist/Pulse-0.2.4-windows-x64.zip.

## 2026-10-12 — Pulse 0.2.5: metronome boost up to 200%

- Practice tools now has a 0–200% metronome slider alongside the numeric volume field. Both remain enabled during playback; drag updates immediately for the next click, typing uses a 250 ms debounce, Enter/blur commits. Invalid input restores the last valid level on blur. Controls synchronize after editing and loading presets. Windows master volume remains separate.
- Audio worker reads an atomic volume value at each click. 200% doubles pre-limiter amplitude relative to 100%. The synth smoothly limits summed magnitudes above 0.8 with a tanh knee, bounded at 0.95, instead of hard-clipping overlapping peaks. Normal low-level output is unchanged.
- PULSE 4 presets store volumes up to 200; loader accepts versions 1–4. Existing default volume remains 70%.
- Core suite: 89,214 checks passed under ASan/UBSan, including increased synthesized energy for all three sounds at 200%, finite/bounded boosted output, 64 overlapping boosted voices, zero-volume silence, boosted preset round-trip and invalid limits. Warning-clean Linux MinGW build passed.
- Real Windows SendInput test passed dragging to 200% and 0% during playback, enabled controls, typed 150% synchronization, invalid-value restoration, continued audio-clock beat notifications and volume retention after stop. Test uses silent beats and no overlay to avoid loud test playback; Windows volume was not changed. Development-aware validation, not independent listening assessment. Log: build/windows-volume_boost_test.log.
- Existing Windows UI regression suite passed; inspected updated Practice tools screenshot for layout. Installed and opened 0.2.5 on Practice tools, verified the visible boost slider has maximum 200, Windows volume and accent controls remain available, and StartApps/shortcuts point to the updated executable. Existing practice windows were preserved. Installed SHA-256: 753c74457be42664a96bb727117de532693671246ac40284bcd7a142a9f93c69. Portable archive: dist/Pulse-0.2.5-windows-x64.zip.

## 2026-10-12 — Pulse 0.2.6: 400% boost and fuller click

- Extended the live metronome volume slider, input validation and audio gain to 0–400%. Windows volume remains separate at 0–100%; existing default metronome volume is unchanged. Existing soft peak limiter retained.
- Added Full click — longer as a fourth sound: 0.75× base pitch with fundamental/second/third harmonic weights 0.72/0.20/0.08, slower exponential decay and a 100 ms tail with a final 20 ms fade to zero. Original sounds retain their 50 ms tails. No-accent mode produces identical waveforms for every click type with the new sound too.
- Sound selection now remains enabled during playback. An atomic sound value reaches the audio worker for the next click; existing voices finish normally. PULSE 5 saves the new sound and volume range; versions 1–4 remain readable.
- Warning-clean Linux MinGW build passed. Core suite: 189,732 ASan/UBSan checks passed, including new sound duration/end, identical unaccented waveforms, preset compatibility/round-trips, zero volume, all four sounds at 400%, and 64 overlapping voices within the limiter bound.
- Synthesized non-accented click energy (sum of squares at 48 kHz), 200%→400%: clean 31.53→107.36; woodblock 57.13→169.04; bright 49.06→155.57; full 75.71→283.54. Full click has 2.40× clean click energy at equal 200% gain. These are digital signal measurements, not measurements of speaker loudness or listening quality.
- Existing Windows native UI regression suite passed. Physical-drag checks were blocked by Windows foreground activation (three attempts stopped before input). Harness now explicitly reports a skipped physical drag and falls back to trackbar position/WM_HSCROLL notifications when focus cannot be obtained; it never sends mouse input to another window. This fallback checks control wiring and live playback, not physical dragging. Prior 0.2.5 physical drag coverage remains recorded above.
- Final boost UI run passed control-notification checks for 400%/0%, live sound selection, typed volume, invalid restoration and continuing WASAPI beat notifications. Physical drags were explicitly skipped due to foreground restrictions. Log: build/windows-volume_boost_test.log.
- Installed and launched 0.2.6 on Practice tools. Verified installed slider maximum 400, four sound options, accents/Windows volume controls, icon and StartApps registration. Existing practice windows preserved. Installed SHA-256: 8151689a535ea5d2fbfb95efdfc613884dc3de8ed7b439feb53095c57278cc25. Portable archive: dist/Pulse-0.2.6-windows-x64.zip.

## 2026-10-12 — Investigating reported differences with accents disabled

- User reports an unspecified effect with accents off; asked whether this means audible loudness/timbre variation, rhythmic wobble, or visual beat highlighting. Clarification pending; no speculative product changes made.
- Offline render probe at 120 BPM, full click/400%, accents off: main-beat window energies identical (283.541 each) with second pulse off. With a 3-per-bar second pulse, the coincident first beat has energy 664.01, versus 283.541 for other main beats. Layering can therefore create an audible emphasis despite equal per-click settings, but this is not established as the user's issue.
- Read-only Windows inspection: running playback is Pulse 0.2.5, 160 BPM, clean tone, metronome volume 200%, accents off, second pulse 0, silent beats 0. Pulse 0.2.6 is also open but stopped at defaults. The second-pulse hypothesis does not explain the inspected current setup. No windows, playback, volume or user settings were changed.

## 2026-10-12 — Pulse 0.2.7: close without an unsaved-changes prompt

- At the user's request, WM_CLOSE now stops audio and destroys the window immediately, even when practice settings are unsaved. This covers the title-bar Close button and Alt+F4. Saving presets remains manual; confirmation before replacing an unsaved practice through Open/example is unchanged.
- Updated the existing native UI smoke check to require clean shutdown with unsaved changes and no dialog interaction. No rhythm or audio synthesis changes.
- Warning-clean Linux MinGW build passed. Real Windows native UI regression suite passed, including immediate clean exit with unsaved changes and no confirmation dialog.
- Installed 0.2.7 and updated desktop/Start menu shortcuts. Installed binary hash verified: b523b58490b3cd87a556be71e72e4fd8b91f63c06ab5652d96601fa32ca13f12. Portable archive: dist/Pulse-0.2.7-windows-x64.zip.

## 2026-10-15 — Pulse 0.2.8: thorough accent audit and overlap fix

Resumed the interrupted investigation. Added a complete-stream audit and a real UI/WASAPI loopback harness with before/after recordings, leading-edge-aligned waveform/energy/timing analysis, and positive accent controls. Found and fixed stacked unaccented clicks and long-tail interference; removed misleading visual group accents when disabled. Normal 160 BPM/no-overlay output was uniform before and after, so the measured overlap defect is not asserted to explain every earlier report.

Validation: 272,558 core ASan/UBSan checks; 1,044 full-stream cases (worst spread 0.001907 dB after versus 0.237536 dB before); Windows overlay emphasis reduced from 3.695578 dB to 0.000000 dB, with zero capture discontinuities/gaps, preserved positive accents, and UI regression suite passing. Warning-clean Windows build. Full methods, evidence, test-analyzer correction and limits: docs/ACCENT_AUDIT.md.
- Installed 0.2.8, verified local binary hash a0713cc92a8840c42f5f9e82ee2663fe6a90940020d9ba8f061d6b5e05517570, and updated desktop/Start menu shortcuts. Opened the fixed window on Practice tools and verified accents are OFF. No older Pulse windows were open. The verification script now reuses an existing current-version window to avoid opening duplicate test windows. Portable archive: dist/Pulse-0.2.8-windows-x64.zip.

## 2026-10-15 — GitHub repository

Prepared the first Git commit for Pulse 0.2.8, including native app source, Linux build scripts, Windows installation/verification scripts, examples, regression tests and the detailed accent audit. Build outputs, portable archives and raw audio captures remain excluded by .gitignore. GitHub destination: private repository rokfurlan123/metronome, branch main. No app changes were made for this publication; validation remains the completed 0.2.8 checks above.

## 2026-10-21 — Pulse 0.3.0: Windows overlay widget

- Located the existing private GitHub repository on the user's request and cloned it to `/home/rokdbest/projects/metronome` on this laptop. Preserved the Win32/WASAPI implementation and existing audio-clock rhythm/synthesis code.
- Pulse now starts as a 400×248 logical-pixel, borderless, always-on-top widget. Drag its header; use the shared BPM edit, held +/−, Tap, Start/Stop and Next step controls. Displays up to 32 numbered beats, current meter/sequence step and silent-beat state. Editor/Widget switches preserve settings, playback, editor placement and session widget position; returning to the editor removes layered/topmost/click-through styles.
- Added 100/85/65% opacity, click-through Lock and a registered Ctrl+Alt+P unlock shortcut. If registration fails (including another instance holding the shortcut), locking is disabled. Switching back to the editor always clears click-through. View/position/opacity are session-only and do not change preset format. Close still stops playback without prompting.
- Updated version resources and optional Windows installer to 0.3.0. Installer/test PowerShell scripts resolve the checkout from their own location rather than the other laptop's hardcoded path. Existing regression drivers explicitly open the full editor before checking its controls; installed-app verifier does likewise.
- Validation: `scripts/test.sh` passed 272,558 ASan/UBSan checks. `bash scripts/test-accent-audit.sh` passed 1,044 full-stream cases, worst regular unaccented energy spread 0.00190663 dB (none over 0.01 dB). Warning-clean static Linux MinGW build passed; executable imports Windows system DLLs only. The four additional Windows test drivers also cross-compiled successfully; they were not executed on native Windows.
- Linux Wine development-aware UI smoke suite passed normally and with the unlock shortcut deliberately occupied. Checked default topmost/widget styles and dimensions, child-control containment, header drag hit-testing, opacity values, click-through style/unlock message handler, safe shortcut-conflict handling, editor placement/style restoration, live view switching with playback, tempo controls, structural locking, tab/sequence editing and immediate close. Logs: `build/ui-widget.log`, `build/ui-widget-hotkey-conflict.log`.
- Screenshot caveat: Wine PrintWindow omits child controls, and a screen BitBlt produced black under this WSL desktop. The widget layout image is therefore an explicit WM_PRINT/owner-draw composite for development inspection, not an actual desktop capture. Inspection caught and corrected cramped BPM/Tap controls. Tests verify drag hit-testing and shortcut handler wiring, not physical dragging, OS-delivered global-key input, or mouse routing into another app.
- Installed Linux-only, locally extracted MinGW and Wine test tooling without sudo; paths and versions are recorded in `docs/TOOLCHAIN.md`. No Windows files were copied/modified and no native Windows process was launched. Native Windows rendering, physical click-through/unlock behavior, mixed-DPI multi-monitor movement and exclusive-fullscreen behavior remain unverified; normal topmost windows do not promise exclusive-fullscreen overlays.
- Prepared `dist/Pulse.exe` and `dist/Pulse-0.3.0-windows-x64.zip`. Windows installation/shortcut updates require the user's separate authorization. Source changes are committed and pushed to the existing private repository; build/runtime artifacts remain ignored.

## 2026-10-21 — Authorized Windows installation and launch

- User explicitly approved installing Pulse on Windows and adding shortcuts. Ran the existing installer from this laptop's `\\wsl.localhost\Ubuntu-24.04\home\rokdbest\projects\metronome` share; initial attempt used the other laptop's `Ubuntu` distribution name and made no changes because the script path did not exist. All compilation remained in Linux/WSL.
- Installed `C:\Users\rocky\AppData\Local\Programs\Pulse\0.3.0\Pulse.exe`, created Desktop `Pulse.lnk` and Start menu `Pulse Metronome.lnk`. Installer verified shortcut targets/icons and installed executable SHA-256 `7269b2b77a5a0ab7bbe376a90a0f8b58c70439ea8f56b50d7c8d9c548f669c6f` against the Linux build. Only this app's download-zone marker was cleared; Windows security settings were unchanged.
- Launched the installed executable through its shortcut and ran `verify-installed.ps1`: correct 0.3.0 process/path, embedded icon, StartApps registration, accent checkbox, Windows volume slider, 400% boost limit and four sound choices verified. No older Pulse windows were open. No playback or Windows volume changes were triggered.
- Returned the app to widget mode and verified that the Editor button is visible and full rhythm-editor controls are hidden. Left the installed widget open for the user. This is native Windows installation/control-wiring verification, not physical click-through/hotkey, listening, or mixed-monitor testing.
