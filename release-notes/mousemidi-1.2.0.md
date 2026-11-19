# MouseMIDI 1.2.0

## Added
- Wheel now also scrolls the view, on top of value-stepping. Each notch injects a one-finger vertical
  touch-drag into the touchscreen (the same device/protocol hwremap uses), so the view scrolls like a mouse;
  the +/- step still fires but is harmless when nothing is selected. Self-calibrates to the screen size.
- `wheel_scroll` (on/off), `scroll_pct` (drag length per notch, % of screen height), `scroll_invert`
  (flip direction), and `touch` (touchscreen device) settings.

## Notes
- Experimental: whether an injected one-finger drag pans the Grid (vs. grabbing a note) is device-behaviour
  we're confirming. Set `wheel_scroll 0` to fall back to value-stepping only.
