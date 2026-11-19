#!/bin/sh
# Build + package the MouseMIDI addin (armhf, MPC OS 3.x) into addin/dist/.
# Needs: arm-linux-gnueabihf-gcc, and the Open MPC tools (mpc-vst-plugins) for release_addin.py + catalog_check.py.
# Point MPCTOOLS at your mpc-vst-plugins/tools checkout (default: ../mpc-vst-plugins/tools).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
MPCTOOLS=${MPCTOOLS:-$HERE/../mpc-vst-plugins/tools}
# gnu11 so glibc doesn't emit its C23-only fscanf/strtol symbols (keeps us under the 2.36 catalog cap)
arm-linux-gnueabihf-gcc -std=gnu11 -D_TIME_BITS=32 -U_FORTIFY_SOURCE -shared -fPIC -O2 -Wall -Wextra \
  -o "$HERE/package/midimouse.so" "$HERE/../lab/mouse/midimouse.c" -lpthread -ldl
rm -f "$HERE"/dist/*.zip
python3 "$MPCTOOLS/release_addin.py" --dir "$HERE/package" --version "${1:-1.0.0}" \
  --repo 3dthc/MPC-MIDI-Converter --license MIT \
  --about "Drive MPC editing from a USB mouse: wheel steps values, middle+scroll edits note length, right-click erases." \
  -o "$HERE/dist"
python3 "$MPCTOOLS/catalog_check.py" "$HERE"/dist/*.zip --catalog
