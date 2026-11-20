#!/usr/bin/env bash
# The smoke test uses only its worktree-local Wine prefix.
set -euo pipefail
# Read back real pixels only inside our own virtual display, never the user's desktop.
exec xvfb-run -a -s '-screen 0 1920x1080x24' env PULSE_PRIVATE_DISPLAY_CAPTURE=1 bash "$(dirname "$0")/test-startup.sh" "${1:---full}"
