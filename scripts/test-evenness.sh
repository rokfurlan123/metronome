#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
python="${PULSE_AUDIO_PYTHON:-/home/rokdbest/tools/pulse-audio-venv/bin/python}"
output="${1:-build/evenness-after}"
mkdir -p build "$output"
"$python" -c 'import numpy, scipy, matplotlib'
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror tests/evenness_audit.cpp -o build/evenness_audit
"$python" tests/test_evenness_analyzer.py
build/evenness_audit "$output"
"$python" scripts/analyze-evenness.py "$output" --expect-flat
"$python" scripts/verify-evenness-ffmpeg.py "$output" --expect-flat
# Compile the native test in Linux, but run it only via the explicit Windows runner.
"${PULSE_MINGW_ROOT:-/home/rokdbest/tools/mingw/usr}/bin/x86_64-w64-mingw32-g++-posix" -std=c++20 -O2 -static -Wall -Wextra -Werror -D_WIN32_WINNT=0x0A00 tests/windows_evenness_audit.cpp -o build/windows_evenness_audit.exe -lgdi32 -lole32 -luuid -lavrt -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme -lmmdevapi
