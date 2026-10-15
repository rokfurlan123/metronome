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
