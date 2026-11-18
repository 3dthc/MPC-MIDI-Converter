#!/bin/sh
# Run MPC once with the pathlog preload, to see where it looks for device scripts. On the MPC:
#   sh pathlog.sh on     restarts MPC with pathlog (save your project first); replug a MIDI device afterwards
#   sh pathlog.sh show   prints what MPC looked up
#   sh pathlog.sh off    restarts MPC without it (a reboot also removes it: the setting lives in /run)
set -eu
SO=/data/mpc-addins/pathlog/pathlog.so
UNIT=$(systemctl status "$(pidof MPC)" 2>/dev/null | head -n 1 | awk '{print $2}'); [ -n "$UNIT" ] || UNIT=inmusic-mpc.service
DROP=/run/systemd/system/$UNIT.d
case "${1:-show}" in
on)
  [ -f /tmp/pathlog.so ] && mkdir -p /data/mpc-addins/pathlog && mv /tmp/pathlog.so "$SO"
  [ -f "$SO" ] || { echo "copy pathlog.so to /tmp first"; exit 1; }
  CUR=$(tr '\0' '\n' < /proc/$(pidof MPC)/environ | sed -n 's/^LD_PRELOAD=//p')
  mkdir -p "$DROP"
  printf '[Service]\nEnvironment="LD_PRELOAD=%s%s%s"\n' "$CUR" "${CUR:+:}" "$SO" > "$DROP/99-pathlog.conf"
  rm -f /tmp/mpc-pathlog.txt
  systemctl daemon-reload && systemctl restart "$UNIT"
  echo "MPC restarted with pathlog. When it is up, unplug and replug your MIDI device, then: sh $0 show" ;;
show)
  sort /tmp/mpc-pathlog.txt 2>/dev/null | uniq -c | sort -rn | head -n 80 ;;
off)
  rm -f "$DROP/99-pathlog.conf"; systemctl daemon-reload && systemctl restart "$UNIT"; echo "MPC restarted without pathlog" ;;
esac
