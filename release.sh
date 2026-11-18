#!/usr/bin/env bash
# Builds, packages, checks and (with --publish) uploads a release, so every release is made the same way.
#   ./release.sh 1.0.1              build + test + zip in dist/ + catalog check
#   ./release.sh 1.0.1 --publish    the same, then create GitHub release v1.0.1 (or replace its zip) with
#                                   release-notes/1.0.1.md as the notes
# Expects mpc-vst-plugins next to this folder (or MV=/path). It must match the newest Open MPC toolchain: an older
# copy makes zips that newer MPC OS versions won't list (no plugin.json), so the script stops if it is behind.
set -euo pipefail
VER="${1:?usage: ./release.sh <version> [--publish]}"
PUBLISH="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
MV="$(cd "${MV:-$HERE/../mpc-vst-plugins}" && pwd)"
UPSTREAM=https://github.com/open-mpc/mpc-vst-plugins
cd "$HERE"

# 1. The toolchain must be the newest one
echo "== checking the Open MPC toolchain"
git -C "$MV" fetch -q "$UPSTREAM" main
if ! git -C "$MV" merge-base --is-ancestor FETCH_HEAD HEAD; then
  echo "Your mpc-vst-plugins is behind Open MPC. Update it first:"
  echo "  git -C \"$MV\" pull --no-rebase $UPSTREAM main"
  echo "(if it says local changes would be overwritten: git -C \"$MV\" stash, run the pull, then git -C \"$MV\" stash show -p to see what was set aside)"
  exit 1
fi

# 2. Version: vst.json must say the same version (MPC shows it)
want=$(python3 -c "v='$VER'.split('.'); print(int(v[0])*1000+int(v[1])*100+int(v[2])*10)")
have=$(python3 -c "import json; print(json.load(open('vst/vst.json'))['version'])")
if [ "$want" != "$have" ]; then
  echo "vst/vst.json says version $have; $VER is $want. Set \"version\": $want in vst/vst.json and commit it."
  exit 1
fi
[ -z "$PUBLISH" ] || [ -f "release-notes/$VER.md" ] || { echo "Write release-notes/$VER.md first (what changed, what was fixed)."; exit 1; }

# 3. Tests and build
echo "== tests and build"
./build.sh

# 4. Package: one zip per CPU (Gen1/Force armv7, Gen2 aarch64), plus one SDK-format folder for every MPC (MPC OS 3.10.1+)
echo "== packaging"
rm -rf dist
for so in vst/build/mpc_midi_converter.so vst/build/aarch64/mpc_midi_converter.so; do
  python3 "$MV/tools/release.py" --so "$so" --skin "vst/build/skin/3dthc - VST - MPC MIDI Converter" \
    --entry vst/build/pluginlist-entry.xml --version "$VER" --about "Converts MIDI files to MPC patterns on the MPC." \
    --repo 3dthc/MPC-MIDI-Converter --license MIT --id mpc-midi-converter -o dist
done
A32=dist/MPC-MIDI-Converter-$VER-mpc-armv7.zip
A64=dist/MPC-MIDI-Converter-$VER-mpc-aarch64.zip
python3 "$MV/tools/sdk_zip.py" "dist/MPC-MIDI-Converter-$VER-mpc-sdk.zip" "$A32" "$A64"

# 5. Checks: any warning stops the release; Gen1 must also work on MPC OS 2.x
echo "== catalog check"
for z in "$A32" "$A64"; do
  out=$(python3 "$MV/tools/catalog_check.py" "$z" --catalog --expect-id mpc-midi-converter --expect-repo 3dthc/MPC-MIDI-Converter 2>&1) || { echo "$out"; exit 1; }
  echo "$out"
  if echo "$out" | grep -q '^warning:'; then echo "$z has warnings: fix them before releasing."; exit 1; fi
  unzip -l "$z" | grep -q '/plugin.json' || { echo "$z has no plugin.json."; exit 1; }
done
unzip -p "$A32" '*/mpc-plugin.json' | head -c 4000 | grep -q '"2.x"' || { echo "The Gen1 build is not MPC OS 2.x compatible."; exit 1; }

# 6. Publish
if [ "$PUBLISH" = "--publish" ]; then
  if gh release view "v$VER" -R 3dthc/MPC-MIDI-Converter >/dev/null 2>&1; then
    gh release upload "v$VER" dist/*.zip --clobber -R 3dthc/MPC-MIDI-Converter
  else
    gh release create "v$VER" dist/*.zip --title "MPC MIDI Converter $VER" --notes-file "release-notes/$VER.md" -R 3dthc/MPC-MIDI-Converter
  fi
  echo "== published v$VER"
else
  ls -1 dist/*.zip
  echo "== ready. Test the armv7 zip on your MPC, then run: ./release.sh $VER --publish"
fi
