#!/bin/sh
# Capture what MPC reads from the panel (data wheel, buttons, Q-Links). Read-only.
# On the MPC (save your project first; each on/off restarts MPC and may change its IP, so re-SSH after):
#   sh panelsniff.sh on       restart MPC with the sniffer
#   sh panelsniff.sh status   confirm it loaded (run after you re-SSH)
#   sh panelsniff.sh show      print opened devices + captured bytes
#   sh panelsniff.sh off       restart MPC without it
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SO=/data/mpc-addins/panelsniff/panelsniff.so

mpc_pid()  { pidof MPC; }
mpc_unit() { sed -n 's#.*/\([^/]*\.service\)$#\1#p' /proc/"$(mpc_pid)"/cgroup 2>/dev/null | head -n 1; }

case "${1:-show}" in
on)
  SRC=""
  [ -f "$HERE/panelsniff.so" ] && SRC="$HERE/panelsniff.so"
  [ -z "$SRC" ] && [ -f /tmp/panelsniff.so ] && SRC=/tmp/panelsniff.so
  if [ -n "$SRC" ]; then mkdir -p "$(dirname "$SO")"; cp "$SRC" "$SO"; fi
  [ -f "$SO" ] || { echo "panelsniff.so not found (copy it to /tmp or beside this script)"; exit 1; }
  UNIT=$(mpc_unit); [ -n "$UNIT" ] || { echo "could not find MPC's service unit"; exit 1; }
  CUR=$(tr '\0' '\n' < /proc/"$(mpc_pid)"/environ | sed -n 's/^LD_PRELOAD=//p')
  DROP=/run/systemd/system/$UNIT.d
  mkdir -p "$DROP"
  printf '[Service]\nEnvironment="LD_PRELOAD=%s%s%s"\n' "$CUR" "${CUR:+:}" "$SO" > "$DROP/99-panelsniff.conf"
  echo "unit:        $UNIT"
  echo "so:          $SO"
  echo "LD_PRELOAD:  $CUR${CUR:+:}$SO"
  rm -f /tmp/mpc-panel.txt
  systemctl daemon-reload && systemctl restart "$UNIT"
  echo "restarted. Re-SSH to the new IP, then: sh $0 status" ;;
status)
  P=$(mpc_pid); [ -n "$P" ] || { echo "MPC not running yet, wait a few seconds"; exit 1; }
  if tr '\0' '\n' < /proc/"$P"/environ | grep -q panelsniff; then
    echo "LOADED (panelsniff is in MPC's LD_PRELOAD)."
    echo "Now: tap the touchscreen, turn the DATA WHEEL ~5 clicks each way, press buttons, turn a Q-Link. Then: sh $0 show"
  else
    echo "NOT loaded. MPC's LD_PRELOAD is:"; tr '\0' '\n' < /proc/"$P"/environ | sed -n 's/^LD_PRELOAD=/  /p'
    echo "drop-in present?"; ls -l /run/systemd/system/*/99-panelsniff.conf 2>&1
  fi ;;
show)
  if [ ! -s /tmp/mpc-panel.txt ]; then echo "(log empty: sniffer not loaded - run: sh $0 status)"; exit 0; fi
  echo "== sniffer / opened devices"; grep -E "^(==|open )" /tmp/mpc-panel.txt | sort -u
  echo "== byte messages (count  fd+bytes)"; grep "^fd" /tmp/mpc-panel.txt | sort | uniq -c | sort -rn | head -n 40 ;;
off)
  UNIT=$(mpc_unit)
  rm -f /run/systemd/system/*/99-panelsniff.conf
  systemctl daemon-reload && [ -n "$UNIT" ] && systemctl restart "$UNIT"
  echo "restarted without the sniffer" ;;
esac
