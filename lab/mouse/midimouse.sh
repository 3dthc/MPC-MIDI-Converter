#!/bin/sh
# Make a USB mouse drive MPC editing: wheel = +/- (value step), middle-hold = Shift, right-click = Erase.
# Reversible: everything lives in a /run drop-in that a clean restart removes. Save your project first;
# each on/off restarts MPC and may change its IP, so re-SSH after.
#   sh midimouse.sh on       restart MPC with the mouse addin
#   sh midimouse.sh status   confirm it loaded (run after you re-SSH)
#   sh midimouse.sh off       restart MPC without it
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SO=/data/mpc-addins/midimouse/midimouse.so

mpc_pid()  { pidof MPC; }
mpc_unit() { sed -n 's#.*/\([^/]*\.service\)$#\1#p' /proc/"$(mpc_pid)"/cgroup 2>/dev/null | head -n 1; }
base_preload() {
  tr '\0' '\n' < /proc/"$(mpc_pid)"/environ | sed -n 's/^LD_PRELOAD=//p' \
    | tr ':' '\n' | grep -v -E '/(pathlog|midicap|midimouse)/' | grep -v '^$' | tr '\n' ':' | sed 's/:$//'
}
drop_clean() { rm -f /run/systemd/system/*/zz-midimouse.conf 2>/dev/null; }

case "${1:-status}" in
on)
  SRC=""
  [ -f "$HERE/midimouse.so" ] && SRC="$HERE/midimouse.so"
  [ -z "$SRC" ] && [ -f /tmp/midimouse.so ] && SRC=/tmp/midimouse.so
  [ -n "$SRC" ] && { mkdir -p "$(dirname "$SO")"; cp "$SRC" "$SO"; }
  [ -f "$SO" ] || { echo "midimouse.so not found (copy it to /tmp or beside this script)"; exit 1; }
  UNIT=$(mpc_unit); [ -n "$UNIT" ] || { echo "could not find MPC's service unit"; exit 1; }
  BASE=$(base_preload)
  drop_clean
  DROP=/run/systemd/system/$UNIT.d
  mkdir -p "$DROP"
  printf '[Service]\nEnvironment="LD_PRELOAD=%s%s%s"\n' "$BASE" "${BASE:+:}" "$SO" > "$DROP/zz-midimouse.conf"
  echo "unit:        $UNIT"
  echo "LD_PRELOAD:  $BASE${BASE:+:}$SO"
  echo "config:      /data/midimouse.conf (optional; defaults: minus 0x37 plus 0x36 shift 0x31 erase 0x09 mouse /dev/input/event2)"
  systemctl daemon-reload && systemctl restart "$UNIT"
  echo "restarted. Re-SSH to the new IP, then: sh $0 status" ;;
status)
  P=$(mpc_pid); [ -n "$P" ] || { echo "MPC not running yet, wait a few seconds"; exit 1; }
  if tr '\0' '\n' < /proc/"$P"/environ | grep -q midimouse; then
    echo "LOADED. Open a sequence in the Grid/track view, then try the mouse:"
    echo "  - scroll wheel         => value steps up/down (+ / -)"
    echo "  - tap middle (latch Shift) => then left-click menu options, or scroll for note length"
    echo "  - right-click          => Erase"
    echo "Pointer and left-click still work as normal (the addin never grabs the mouse)."
  else
    echo "NOT loaded. LD_PRELOAD:"; tr '\0' '\n' < /proc/"$P"/environ | sed -n 's/^LD_PRELOAD=/  /p'
    echo "drop-ins:"; ls -l /run/systemd/system/*/zz-midimouse.conf 2>&1
  fi ;;
off)
  UNIT=$(mpc_unit)
  drop_clean
  systemctl daemon-reload && [ -n "$UNIT" ] && systemctl restart "$UNIT"
  echo "restarted clean (midimouse removed)" ;;
esac
