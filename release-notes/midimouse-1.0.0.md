# MIDIMouse 1.0.0

MIDIMouse turns a USB mouse into a hardware-style editor for Akai MPC standalone devices. It runs as an Open MPC
addin, reads your mouse directly, and injects the matching MPC panel controls - no extra hardware, and it loads
automatically on boot.

## What it does
- Wheel - steps the selected value / note length, with acceleration (a fast spin jumps further)
- Middle button - latches Shift (tap on, tap off), so you can left-click the on-screen Shift options
- Right-click - cycles the Grid tools: pencil -> eraser -> select -> zoom
- Rear side button - Stop; front side button - Play
- Left-click and pointer stay native (select as normal)

## Highlights
- Auto-detects any mouse, hot-plug included, so you can swap mice freely
- Works with 3-button and 5-button mice (side-button actions just don't fire on a 3-button)
- Fully configurable live, no restart: remap any button (none / shift / erase / play / stop / rec / menu / main /
  tool), tune wheel acceleration, note numbers, and the tool-icon positions
- Standard Open MPC addin: permanent across reboots, with a clean uninstall

## Notes
- Panel notes and tool-icon coordinates were captured on an MPC Live II; other models may need the tool
  coordinates nudged in the config.
