# Pulse — local design review, 2026-10-31

Before: source revision `c89617d` (Pulse 0.3.1), captured from a baseline source snapshot **inside the local worktree**, not by running tests on main.
After: `pulse/design-audio` development branch. The installed Windows app/shortcuts remain unchanged. Codex does not use Linear for Pulse.

## Compare

- [Tiny bar and compact overlay — before/after](overlays-comparison.png)
- [Rhythm editor — before/after](rhythm-comparison.png)
- [Sequence editor — before/after](sequence-comparison.png)
- [Practice tools — before/after](tools-comparison.png)
- [Presets dropdown open](after/presets.png)

The original, unscaled PNGs are in `before/` and `after/`. This is an intentional change from the previous light-only palette to a single graphite/lime palette, not a new light/dark switch.

## Capture limits

These are client-area captures of the actual native C++ app running in Linux-hosted Wine at 96 DPI. Wine omits some child controls/menu rows from WM_PRINT; the capture harness composites their actual native owner-draw/WM_PRINT output into the client capture. They are layout-review evidence, **not physical desktop screenshots, browser mockups, independent testing, real Windows validation, or audio listening evidence**. Native font substitution can differ from Windows Segoe UI.

Pulse has desktop tiny/compact/editor views, not browser phone/tablet layouts. Responsively App is not applicable. No Windows-drive files were modified, no installed app was replaced, no Windows master-volume change was triggered. Captures were made before any Git commit/push; subsequent branch publication does not install or release the application.

## Reproduce (Linux/WSL)

```bash
bash scripts/preview-native.sh docs/design-review/before baseline
bash scripts/preview-native.sh docs/design-review/after
python3 scripts/design-review.py
```

Requires the existing extracted Linux MinGW/Wine tools documented in `../TOOLCHAIN.md`, plus the already-installed Linux Pillow package and DejaVu font. Do not run Wine suites simultaneously; they use the same disposable Linux prefix.

### Existing Wine placement quirk

A slower, synchronized size-cycle check exposes editor frame-position drift in this WSLg/Wine environment. It reproduces identically in the baseline and current build: `(6,27) → (12,54) → (18,81)` with constant 908×644 outer dimensions. See [baseline trace](placement-before.txt) and [current trace](placement-after.txt). The UI harness warns about this only under Wine; its native Windows placement assertion remains strict. This is not claimed fixed and is not evidence of a new design regression.

## Local checks

- Core ASan/UBSan: **272,573 checks passed**.
- Rendered audio: **1,080 cases**, including 1,068 unaccented cases with zero aligned sample difference and zero energy spread; 12 positive accent controls remain accented.
- Native UI: **164 checks passed**, with the existing baseline-matching Wine placement warning above. Includes all four menu choices, a saved-file menu entry, live controls and simulated 144/288/96 DPI changes.
- Actual mouse volume drag: **12 checks passed**. The additional physical-keyboard tempo harness lost foreground focus and did not complete; no pass is claimed for it.
- Warning-clean Linux cross-build and `git diff --check` passed. Native Windows listening/mixed-monitor/installation checks remain pending.
