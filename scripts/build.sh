#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_dir="${PULSE_MINGW_ROOT:-/home/rokdbest/tools/mingw/usr}"
compiler="$toolchain_dir/bin/x86_64-w64-mingw32-g++-posix"
export PATH="$toolchain_dir/bin:$PATH"
mkdir -p "$project_dir/dist" "$project_dir/build"
"$toolchain_dir/bin/x86_64-w64-mingw32-windres" --preprocessor "$compiler" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I "$project_dir/src" -I "$toolchain_dir/x86_64-w64-mingw32/include" "$project_dir/src/app.rc" -O coff -o "$project_dir/build/app.res"
"$compiler" -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -static -static-libgcc -static-libstdc++ -municode -mwindows \
    -D_WIN32_WINNT=0x0A00 -I "$project_dir/src" "$project_dir/src/main.cpp" "$project_dir/build/app.res" \
    -o "$project_dir/build/Pulse.exe" -lole32 -luuid -lavrt -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme
# Replace the finished executable atomically; never truncate an open app image.
python3 -c 'import os, sys; os.replace(sys.argv[1], sys.argv[2])' "$project_dir/build/Pulse.exe" "$project_dir/dist/Pulse.exe"
printf 'Built %s\n' "$project_dir/dist/Pulse.exe"
