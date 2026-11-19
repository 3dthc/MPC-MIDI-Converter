# MouseMIDI 1.4.0

## Added
- Mouse hot-plug detection: a mouse plugged in after MPC has started is now picked up automatically
  (previously only mice present at startup were read).
- Wheel acceleration (`wheel_accel`, default on): a fast spin steps further per notch, like the data
  wheel, so value and note-length changes keep up instead of crawling one tick at a time.
- Stop and Rec as mappable actions (notes 0x51 / 0x52, captured on hardware).

## Changed
- Rear side button (BTN_SIDE) now defaults to Stop (was Shift). Middle button keeps Shift, front side
  button keeps Play.
