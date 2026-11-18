#!/bin/sh
# Find which input device the data wheel / buttons use. On the MPC:
#   sh evscan.sh            lists the input devices and their names
#   sh evscan.sh <dev>      reads one device for 5s - operate the control while it runs
# Needs ./evread beside this script (build on the PC, scp over).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
if [ $# -ge 1 ]; then
  echo "Operate the DATA WHEEL (and a button) now, for 5 seconds:"
  timeout 5 "$HERE/evread" "$1"; exit 0
fi
echo "== input devices"
cat /proc/bus/input/devices | awk '/^N:/{n=$0} /^H:/{print n"  "$0}'
echo
echo "Then run, trying each, while turning the data wheel:"
echo "  sh $0 /dev/input/event0   # touchscreen"
echo "  sh $0 /dev/input/event1   # gpio-keys / panel"
echo "  sh $0 /dev/input/event2   # mouse"
