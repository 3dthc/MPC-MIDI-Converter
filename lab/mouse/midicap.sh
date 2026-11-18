#!/bin/sh
# Capture what MPC reads from the panel (data wheel, buttons, Q-Links). Read-only.
# On the MPC (save your project first; each on/off/clean restarts MPC and may change its IP, so re-SSH after):
#   sh midicap.sh on       restart MPC with the sniffer
#   sh midicap.sh status   confirm it loaded (run after you re-SSH)
#   sh midicap.sh show      print opened devices + captured bytes
#   sh midicap.sh off       restart MPC without it (also clears the old pathlog leftover)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SO=/data/mpc-addins/midicap/midicap.so

mpc_pid()  { pidof MPC; }
mpc_unit() { sed -n 's#.*/\([^/]*\.service\)$#\1#p' /proc/"$(mpc_pid)"/cgroup 2>/dev/null | head -n 1; }
# current LD_PRELOAD with our lab libraries (pathlog, midicap) stripped out
base_preload() {
  tr '\0' '\n' < /proc/"$(mpc_pid)"/environ | sed -n 's/^LD_PRELOAD=//p' \
    | tr ':' '\n' | grep -v -E '/(pathlog|midicap)/' | grep -v '^$' | tr '\n' ':' | sed 's/:$//'
}
drop_clean() { rm -f /run/systemd/system/*/??-pathlog.conf /run/systemd/system/*/??-midicap.conf /run/systemd/system/*/zz-midicap.conf 2>/dev/null; }

case "${1:-show}" in
on)
  SRC=""
  [ -f "$HERE/midicap.so" ] && SRC="$HERE/midicap.so"
  [ -z "$SRC" ] && [ -f /tmp/midicap.so ] && SRC=/tmp/midicap.so
  [ -n "$SRC" ] && { mkdir -p "$(dirname "$SO")"; cp "$SRC" "$SO"; }
  [ -f "$SO" ] || { echo "midicap.so not found (copy it to /tmp or beside this script)"; exit 1; }
  UNIT=$(mpc_unit); [ -n "$UNIT" ] || { echo "could not find MPC's service unit"; exit 1; }
  BASE=$(base_preload)
  drop_clean
  DROP=/run/systemd/system/$UNIT.d
  mkdir -p "$DROP"
  # zz- so it sorts after the addins' own drop-in and wins the LD_PRELOAD assignment
  printf '[Service]\nEnvironment="LD_PRELOAD=%s%s%s"\n' "$BASE" "${BASE:+:}" "$SO" > "$DROP/zz-midicap.conf"
  echo "unit:        $UNIT"
  echo "LD_PRELOAD:  $BASE${BASE:+:}$SO"
  rm -f /tmp/mpc-midicap.txt
  systemctl daemon-reload && systemctl restart "$UNIT"
  echo "restarted. Re-SSH to the new IP, then: sh $0 status" ;;
status)
  P=$(mpc_pid); [ -n "$P" ] || { echo "MPC not running yet, wait a few seconds"; exit 1; }
  if tr '\0' '\n' < /proc/"$P"/environ | grep -q midicap; then
    echo "LOADED. Now: tap the touchscreen, turn the DATA WHEEL ~5 clicks each way, then press Shift, Erase, and a couple of buttons. Then: sh $0 show"
  else
    echo "NOT loaded. LD_PRELOAD:"; tr '\0' '\n' < /proc/"$P"/environ | sed -n 's/^LD_PRELOAD=/  /p'
    echo "drop-ins:"; ls -l /run/systemd/system/*/zz-midicap.conf 2>&1
  fi ;;
show)
  if [ ! -s /tmp/mpc-midicap.txt ]; then echo "(log empty: run sh $0 status)"; exit 0; fi
  echo "== ports opened"; grep -E "^(==|open |->)" /tmp/mpc-midicap.txt | sort -u
  echo "== control MIDI (count  bytes), most common first"; grep "^in:" /tmp/mpc-midicap.txt | sort | uniq -c | sort -rn | head -n 50 ;;
off)
  UNIT=$(mpc_unit)
  drop_clean
  systemctl daemon-reload && [ -n "$UNIT" ] && systemctl restart "$UNIT"
  echo "restarted clean (pathlog + midicap removed)" ;;
esac
