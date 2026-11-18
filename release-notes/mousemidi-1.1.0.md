# MouseMIDI 1.1.0

## Added
- `shift_mode` setting (`latch` or `hold`). Default is now `latch`: tap the middle button to toggle Shift on,
  tap again to toggle off.
- `shift_timeout` setting: a latched Shift auto-releases after this many idle seconds (default 12; 0 = never),
  so it can't get stuck on.

## Fixed
- Holding the middle button for Shift blocked left-click, so you couldn't pick the on-screen options a held
  Shift brings up. MPC maps every mouse button to a single touch contact, so any held button suppresses the
  tap. Latching Shift leaves only the left button pressed at click time, so Shift + left-click now works.

## Notes
- `hold` mode (the 1.0.0 behaviour) is still available for scroll-only use (middle-hold + wheel = note length).
