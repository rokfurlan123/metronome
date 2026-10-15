#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$project_dir/build"
g++ -std=c++20 -O2 -Wall -Wextra -Werror "$project_dir/tests/accent_audit.cpp" -o "$project_dir/build/accent_audit"
"$project_dir/build/accent_audit" "$project_dir/build/accent-audit-after.csv"
