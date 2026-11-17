#!/usr/bin/env bash
# Builds the MPC plugin (armhf .so + skin) with the Open MPC toolchain. Needs Docker.
#   ./build.sh            (expects mpc-vst-plugins next to this folder, or set MV=/path/to/mpc-vst-plugins)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MV="${MV:-$HERE/../mpc-vst-plugins}"
[ -x "$MV/tools/build_port.sh" ] || { echo "mpc-vst-plugins not found at $MV (set MV=...)"; exit 1; }
"$MV/tools/test_port.sh" "$HERE/vst/vst.json"
"$HERE/test/run_tests.sh"
"$MV/tools/build_port.sh" "$HERE/vst/vst.json"
