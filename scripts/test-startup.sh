#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
runtime=/home/rokdbest/tools/pulse-test
mkdir -p build
compiler="${PULSE_MINGW_ROOT:-/home/rokdbest/tools/mingw/usr}/bin/x86_64-w64-mingw32-g++-posix"
"$compiler" -std=c++20 -O2 -static -Wall -Wextra -Werror -D_WIN32_WINNT=0x0A00 tests/ui_smoke.cpp -o build/ui_startup.exe -lgdi32 -lole32 -luuid -lavrt -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme
# A task-local prefix: never stop or reuse another assistant's Wine sessions.
export DISPLAY="${DISPLAY:-:0}" WINEDEBUG=-all WINEPREFIX="$project_dir/build/wine-startup"
export WINESERVER="$runtime/usr/lib/wine/wineserver64" WINELOADER="$runtime/usr/lib/wine/wine64"
export WINEDLLPATH="$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-windows:$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-unix"
export LD_LIBRARY_PATH="$runtime/usr/lib/x86_64-linux-gnu:$runtime/usr/lib/x86_64-linux-gnu/pulseaudio"
mkdir -p "$WINEPREFIX/drive_c/windows/system32"
cp "$runtime/usr/x86_64-w64-mingw32/lib/zlib1.dll" "$WINEPREFIX/drive_c/windows/system32/zlib1.dll"
trap '"$WINESERVER" -k || true' EXIT
"$WINELOADER" "${PULSE_STARTUP_EXE:-build/ui_startup.exe}" "${1:---startup-only}" 2>build/wine-startup.log
