#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
runtime=/home/rokdbest/tools/pulse-test
for prefix in "$runtime/prefix" "$runtime/prefix-v2" "$runtime/prefix-v3"; do
    WINEPREFIX="$prefix" "$runtime/usr/lib/wine/wineserver64" -k || true
done
trap 'WINEPREFIX="$runtime/prefix-v3" "$runtime/usr/lib/wine/wineserver64" -k || true' EXIT
"${PULSE_MINGW_ROOT:-/home/rokdbest/tools/mingw/usr}/bin/x86_64-w64-mingw32-g++-posix" -std=c++20 -O2 -static -Wall -Wextra -Werror -D_WIN32_WINNT=0x0A00 tests/ui_smoke.cpp -o build/ui_smoke.exe -lgdi32 -lole32 -luuid -lavrt -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme
export DISPLAY=:0 WINEDEBUG=-all WINEPREFIX="$runtime/prefix-v3"
export WINESERVER="$runtime/usr/lib/wine/wineserver64" WINELOADER="$runtime/usr/lib/wine/wine64"
export WINEDLLPATH="$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-windows:$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-unix"
export LD_LIBRARY_PATH="$runtime/usr/lib/x86_64-linux-gnu:$runtime/usr/lib/x86_64-linux-gnu/pulseaudio"
"$runtime/usr/lib/wine/wine64" build/ui_smoke.exe 2>build/wine.log
