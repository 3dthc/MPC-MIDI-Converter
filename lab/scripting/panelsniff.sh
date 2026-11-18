#!/bin/sh
# Capture what MPC reads from the panel. On the MPC (save your project first; this restarts MPC, not the device):
#   sh panelsniff.sh on     restart MPC with the sniffer, then operate the data wheel/buttons
#   sh panelsniff.sh show    print what the panel sent (grouped)
#   sh panelsniff.sh off     restart MPC without it (a reboot also removes it: it lives in /run)
set -eu
SO=/data/mpc-addins/panelsniff/panelsniff.so
UNIT=$(systemctl status "$(pidof MPC)" 2>/dev/null | head -n 1 | awk '{print $2}'); [ -n "$UNIT" ] || UNIT=inmusic-mpc.service
DROP=/run/systemd/system/$UNIT.d
case "${1:-show}" in
on)
  [ -f /tmp/panelsniff.so ] && { mkdir -p /data/mpc-addins/panelsniff; mv /tmp/panelsniff.so "$SO"; }
  [ -f "$SO" ] || { echo "copy panelsniff.so to /tmp first"; exit 1; }
  CUR=$(tr '\0' '\n' < /proc/$(pidof MPC)/environ | sed -n 's/^LD_PRELOAD=//p')
  mkdir -p "$DROP"
  printf '[Service]\nEnvironment="LD_PRELOAD=%s%s%s"\n' "$CUR" "${CUR:+:}" "$SO" > "$DROP/99-panelsniff.conf"
  rm -f /tmp/mpc-panel.txt
  systemctl daemon-reload && systemctl restart "$UNIT"
  echo "MPC restarting. When it is up: turn the DATA WHEEL slowly ~5 clicks each way, press a few buttons and a Q-Link, then: sh $0 show" ;;
show)
  if [ ! -s /tmp/mpc-panel.txt ]; then echo "(nothing captured: the wheel/buttons are not read from these devices)"; exit 0; fi
  echo "== distinct messages (count  fd+bytes)"; sort /tmp/mpc-panel.txt | uniq -c | sort -rn | head -n 40
  echo "== which fds"; cut -d: -f1 /tmp/mpc-panel.txt | sort | uniq -c ;;
off)
  rm -f "$DROP/99-panelsniff.conf"; systemctl daemon-reload && systemctl restart "$UNIT"; echo "MPC restarted without the sniffer" ;;
esac
