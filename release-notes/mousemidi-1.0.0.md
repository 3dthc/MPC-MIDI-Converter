# MouseMIDI 1.0.0

First release. A USB mouse now drives MPC standalone editing like a desktop DAW.

## Added
- Scroll wheel steps the selected value / scrolls lists (acts like the +/- buttons).
- Hold the middle button and scroll to edit note length (Shift + value step).
- Right-click acts as Erase.
- Pointer and left-click stay fully native — the addin never grabs the mouse, it only reads
  the wheel and right/middle buttons that MPC otherwise ignores, and injects the matching panel MIDI.
- `/data/mpc-addins/mousemidi/mousemidi.conf` to retune without a rebuild: mouse device, the +/-/Shift/Erase
  note numbers, scroll invert, a jog-CC mode, and which buttons map to Shift/Erase.
- Installs as a standard Open MPC addin (persists across reboots; clean uninstall).

## Notes
- Built for MPC OS 3.x (Gen1/Gen2). Panel note numbers default to those captured on an MPC Live II.
