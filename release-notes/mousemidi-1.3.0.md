# MouseMIDI 1.3.0

## Added
- 5-button mouse support with per-button actions. Defaults: rear side button = Shift latch,
  front side button = Play. Each button (`btn_middle`, `btn_right`, `btn_side_back`, `btn_side_fwd`)
  takes an action: none | shift | erase | play | menu | main.
- Mouse auto-detection (`mouse auto`): the addin reads every mouse it finds, so a 3-button and a
  5-button mouse both work and swapping between them needs no config.
- Play as a mappable action (panel note 0x53, captured on hardware), plus Menu/Main notes.

## Changed
- Shift is on the middle button *and* the rear side button, so 5-button users get it on the thumb
  (freeing the middle) while 3-button users keep it on the middle.

## Removed
- The experimental wheel-scroll (touch-drag) code. It never panned cleanly and added lag to
  note-length edits; the wheel is value-step only again.
