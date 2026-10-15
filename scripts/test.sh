#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$project_dir/build"
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -I "$project_dir/src" "$project_dir/tests/rhythm_test.cpp" -o "$project_dir/build/rhythm_test"
ASAN_OPTIONS=detect_leaks=0 "$project_dir/build/rhythm_test"
