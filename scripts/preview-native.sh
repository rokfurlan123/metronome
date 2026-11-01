#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build "${1:-build}"
runtime=/home/rokdbest/tools/pulse-test
export DISPLAY=:0 WINEDEBUG=-all WINEPREFIX="$runtime/prefix-v3"
export WINESERVER="$runtime/usr/lib/wine/wineserver64" WINELOADER="$runtime/usr/lib/wine/wine64"
export WINEDLLPATH="$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-windows:$runtime/usr/lib/x86_64-linux-gnu/wine/x86_64-unix"
export LD_LIBRARY_PATH="$runtime/usr/lib/x86_64-linux-gnu:$runtime/usr/lib/x86_64-linux-gnu/pulseaudio"
trap '"$WINESERVER" -k || true' EXIT
preview_flags=()
if [[ "${2:-}" == baseline ]]; then
    mkdir -p build/baseline
    for file in main.cpp audio.h rhythm.h system_volume.h; do git show "c89617d:src/$file" > "build/baseline/$file"; done
    preview_flags=(-DPULSE_BASELINE '-DPULSE_SOURCE_MAIN="../build/baseline/main.cpp"')
fi
"${PULSE_MINGW_ROOT:-/home/rokdbest/tools/mingw/usr}/bin/x86_64-w64-mingw32-g++-posix" -std=c++20 -O2 -static -Wall -Wextra -Werror -D_WIN32_WINNT=0x0A00 "${preview_flags[@]}" tests/design_preview.cpp -o build/design_preview.exe -lgdi32 -lole32 -luuid -lavrt -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme
"$WINELOADER" build/design_preview.exe "${1:-build}" "${2:-menu}" 2>build/preview-wine.log
python3 - "${1:-build}" <<'PY'
import sys
from pathlib import Path
from PIL import Image
for p in Path(sys.argv[1]).glob('*.bmp'):
    Image.open(p).convert('RGB').save(p.with_suffix('.png'))
PY
