#!/usr/bin/env bash
# Converter + plugin engine tests on the build machine (x86, ASan/UBSan). No device needed.
set -euo pipefail
T="$(cd "$(dirname "$0")" && pwd)"; R="$T/.."; MV="$(cd "${MV:-$R/../mpc-vst-plugins}" && pwd)"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 -Wall"
cd "$W"
python3 "$T/make_basic.py" >/dev/null; python3 "$T/make_more.py"
gcc $SAN -o cli "$T/cli.c" "$R/src/convert.c"
mkdir out
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; exit 1; fi; }
check "drum file converts"        './cli test_drums_90bpm.mid out 0 0 0 0 0 1 | grep -q "^1 "'
check "chord file converts"       './cli test_chords_80bpm.mid out >/dev/null'
check "format 1 splits by track"  './cli multi_f1.mid out 1 0 0 0 0 1 | grep -q "^3 "'
check "format 0 splits by channel" './cli multi_f0.mid out 1 0 0 0 0 1 | grep -q "^3 "'
check "not-MIDI is refused"       '! ./cli notmidi.mid out >/dev/null'
check "truncated file keeps notes" './cli truncated.mid out >/dev/null'
python3 - <<'PY'
import json, glob
fs = glob.glob('out/*.mpcpattern'); [json.load(open(f)) for f in fs]
e = json.load(open('out/test_drums_90bpm (90 BPM).mpcpattern'))['pattern']['events']
assert [(x['time'], x['1'], round(x['2'] * 127)) for x in e][:3] == [(0, 36, 120), (0, 42, 100), (480, 42, 70)], e[:3]
assert max(x['time'] for x in e) == 7200
print("ok   %d patterns are valid JSON, drum timing and velocity exact" % len(fs))
PY
hits=0; for f in fuzz/*.mid; do ./cli "$f" out 1 1 240 3 1 1 >/dev/null 2>err || true; grep -q -E 'Sanitizer|runtime error' err && hits=$((hits+1)); done
check "300 damaged files: no memory errors" '[ $hits = 0 ]'
D="$W/root"; mkdir -p "$D/TESTDRIVE/Pack/Sub/Deeper" "$D/az01-internal" "$D/az01-internal-sd"
cp multi_f1.mid multi_f0.mid "$D/TESTDRIVE/Pack/"; cp test_drums_90bpm.mid "$D/TESTDRIVE/Pack/Sub/Drums.MIDI"
cp leadin.mid "$D/TESTDRIVE/Pack/Sub/Deeper/"; cp notmidi.mid "$D/TESTDRIVE/Pack/broken.mid"; echo hi > "$D/TESTDRIVE/Pack/notes.txt"
gcc $SAN -DMMC_ROOT="\"$D\"" -I"$MV/wrapper" -I"$R/src" -o et "$T/engine_test.c" "$R/src/engine.c" "$R/src/convert.c" -lpthread
./et
