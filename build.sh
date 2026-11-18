#!/usr/bin/env bash
# Tests, then builds the plugin for every MPC: Gen1/Force (armv7) and Gen2 (aarch64), with the skin in the MPC OS 2.x
# shape, which MPC OS 3.x reads too, so one build covers OS 2 and OS 3. Needs Docker.
#   ./build.sh            (expects mpc-vst-plugins next to this folder, or set MV=/path/to/mpc-vst-plugins)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MV="${MV:-$HERE/../mpc-vst-plugins}"
[ -x "$MV/tools/build_port.sh" ] || [ -f "$MV/tools/build_port.sh" ] || { echo "mpc-vst-plugins not found at $MV (set MV=...)"; exit 1; }
export SHADOW_SKIN_MPC_OS=2
bash "$MV/tools/test_port.sh" "$HERE/vst/vst.json"
bash "$MV/tools/test_port.sh" "$HERE/vst/vst.json" aarch64
"$HERE/test/run_tests.sh"
bash "$MV/tools/build_port.sh" "$HERE/vst/vst.json" all
