#!/bin/sh
# MPC MIDI Converter scripting lab, step 1. Run on the MPC (Developer Mode shell or SSH):
#   sh lab.sh info      read-only: where MPC keeps its files, MIDI port names, scripting state
#   sh lab.sh install   copies the test script to the places MPC may read user device scripts from
#   sh lab.sh remove    removes every copy again
# Nothing here touches MPC's own files or your projects.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
PID=$(pidof MPC)
MPCHOME=$(tr '\0' '\n' < /proc/$PID/environ 2>/dev/null | sed -n 's/^HOME=//p')
[ -n "$MPCHOME" ] || MPCHOME=/root
CANDIDATES="$MPCHOME/.config/Akai/SME0/Devices/3dthc $MPCHOME/.local/share/Akai/SME0/Devices/3dthc /data/Settings/Devices/3dthc"
for d in /media/*; do case "$d" in */acvs-*|*/az01-internal) ;; *) CANDIDATES="$CANDIDATES $d/Devices/3dthc";; esac; done

case "${1:-info}" in
info)
  echo "== MPC HOME: $MPCHOME"
  echo "== MPC environment"; tr '\0' '\n' < /proc/$PID/environ 2>/dev/null | grep -v -i -E 'pass|token|key' | sort
  echo "== MPC's own folders"; ls -la "$MPCHOME" "$MPCHOME/.config" "$MPCHOME/.config/Akai" "$MPCHOME/.config/Akai/SME0" 2>&1 | head -n 60
  echo "== MIDI ports"; amidi -l 2>&1; cat /proc/asound/seq/clients 2>/dev/null | grep -E 'Client|Port'
  echo "== files MPC has open (non-system)"; ls -l /proc/$PID/fd 2>/dev/null | awk '{print $NF}' | grep -v -E '^(/dev|socket|pipe|anon)' | sort -u | head -n 40
  echo "== scripting flag"; ls -la /data/scriptingDefaultEnabled 2>&1
  ;;
install)
  PORTS=$(amidi -l 2>/dev/null | awk 'NR>1{ $1=""; $2=""; sub(/^ +/,""); print }' | grep -v -i 'MPC' | head -n 1)
  [ -n "$PORTS" ] || { echo "Plug in a USB MIDI keyboard or synth first (the test script attaches to it)."; exit 1; }
  echo "Attaching the test script to: $PORTS"
  for d in $CANDIDATES; do
    mkdir -p "$d" 2>/dev/null || continue
    cp "$HERE/mmcLab.lua" "$d/mmcLab.lua"
    printf '[info]\ndevice=%s\nmanufacturer=3dthc\ninput=%s\noutput=%s\nscript=mmcLab.lua\nscriptVersion=1.0\n' \
      "$PORTS" "$PORTS" "$PORTS" > "$d/mmcLab.ini"
    echo "installed in $d"
  done
  echo "Now unplug and replug that MIDI device. If a popup says 'The test script loaded', scripting works."
  echo "If nothing appears, save your project, restart MPC, replug the device again, then run: sh lab.sh info"
  ;;
remove)
  for d in $CANDIDATES; do rm -f "$d/mmcLab.lua" "$d/mmcLab.ini" 2>/dev/null; rmdir "$d" "$(dirname "$d")" 2>/dev/null; done
  echo removed
  ;;
esac
