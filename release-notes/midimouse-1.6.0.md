# MIDIMouse 1.6.0

## Added
- "tool" button action: cycles the Grid tool icons (pencil -> eraser -> select) by injecting a tap at each,
  so one button steps through them. Icon positions are configurable (`tool1`/`tool2`/`tool3` = "x,y" raw
  touchscreen coords, captured on an MPC Live II) and the touch device is set by `touch`.
- Any button can be set to `tool` (e.g. `btn_right tool`). Default button actions are unchanged.

## Notes
- Experimental: relies on the MPC accepting injected touch taps. If a tap doesn't land, the coords may need
  nudging, or tap injection may not take on a given firmware.
