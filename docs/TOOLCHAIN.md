# Toolchain

All build and test tooling stays in Linux/WSL. No Windows toolchains were installed and no files were copied into a Windows drive.

## Native Windows cross-build

Local root: `/home/rokdbest/tools/mingw/usr`.

Ubuntu Noble packages downloaded with `apt-get download` and extracted with `dpkg-deb -x`, without sudo:

- GCC / G++ MinGW-w64 POSIX: `13.2.0-6ubuntu1+26.1`.
- MinGW-w64 headers: `11.0.1-3build1`.
- Binutils MinGW-w64: `2.41.90.20240122-1ubuntu1+11.4`.
- Related compiler base and POSIX runtime packages are retained in `/home/rokdbest/tools/mingw/packages`.

Compiler: `/home/rokdbest/tools/mingw/usr/bin/x86_64-w64-mingw32-g++-posix`.

The executable statically links C++ and POSIX runtime support and imports only Windows system DLLs. Build flags enable warnings as errors. The embedded manifest requests normal user privileges, common-controls v6, and per-monitor DPI awareness.

## Tests

Core tests: system Linux `g++`, C++20, ASan and UBSan; LeakSanitizer disabled because the execution environment is traced.

Optional GUI runtime: Wine `9.0~repack-4build3`, locally extracted beneath `/home/rokdbest/tools/pulse-test`. The packaged `wineserver` wrapper was adjusted to use that local root, and the package-provided x64 `zlib1.dll` was copied into Wine's DLL directory and its private prefix's system32 directory. Wine writes only into a Linux test prefix. The smoke driver is cross-compiled with the same MinGW compiler.

Xvfb packages were also downloaded locally while evaluating headless testing. The virtual display could not initialize because it expected a system-installed keyboard compiler; it is not needed by the app. GUI testing uses the existing Linux desktop display instead.

## 2026-10-21 — toolchain restored on this laptop

The repository was cloned into `/home/rokdbest/projects/metronome`. Downloaded the same MinGW package versions above with Linux `apt-get download` and extracted them under `/home/rokdbest/tools/mingw` (no sudo). No Windows toolchain was installed.

Restored Wine 9.0~repack-4build3 locally under `/home/rokdbest/tools/pulse-test` using extracted `wine64`, `libwine`, `fonts-wine`, `libz-mingw-w64`, and missing runtime dependency packages. Downloads are retained in that directory's `packages/`. Updated only the extracted `usr/lib/wine/wineserver` wrapper to reference this local root; copied the package's x64 `zlib1.dll` into the Wine DLL directory and disposable Linux prefix's system32, as above. Tests use the existing WSL Linux display and require permission for Wine's local Unix socket outside the restricted sandbox. No native Windows app was launched.

`scripts/test-ui.sh --hotkey-conflict` reserves Ctrl+Alt+P in the harness before starting the GUI, to verify that unsafe click-through is disabled when shortcut registration fails. Widget layout composites are development evidence, not physical desktop screenshots or native Windows validation.
