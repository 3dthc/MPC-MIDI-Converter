# MouseMIDI 1.2.1

## Changed
- Wheel-scroll is now OFF by default (`wheel_scroll 0`). The injected one-finger drag didn't pan the view as
  intended, so the default returns to 1.1.0 behaviour: wheel steps values only. The scroll code stays in as an
  opt-in (`wheel_scroll 1`) for anyone who wants to experiment or tune `scroll_pct` / `scroll_invert`.
