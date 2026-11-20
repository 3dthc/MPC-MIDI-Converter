# MIDIMouse settings

All settings live in `/data/mpc-addins/midimouse/midimouse.conf` on the device (one `key value` per line).
**Edits apply live within ~2 seconds — no restart, no reinstall.** An upgrade keeps your edits.

## Settings

| Key | Values | Meaning |
|-----|--------|---------|
| `mouse` | `auto` or `/dev/input/eventN` | `auto` reads every mouse and watches for hot-plug; or pin one device |
| `btn_middle` | action | middle button (default `shift`) |
| `btn_right` | action | right button (default `tool`) |
| `btn_side_back` | action | rear thumb button, 5-button mice (default `stop`) |
| `btn_side_fwd` | action | front thumb button, 5-button mice (default `play`) |
| `shift_mode` | `latch` or `hold` | latch = tap on/off; hold = only while held |
| `shift_timeout` | seconds | auto-release a latched Shift after N idle seconds (`0` = never) |
| `wheel_accel` | `1`+ | max steps per notch on a fast spin (`1` = off, higher = jumpier) |
| `invert` | `0`/`1` | flip wheel direction |
| `cc` | `0`/`1` | send a jog CC instead of the +/- buttons |
| `tool1`..`tool4` | `x,y` | Grid tool-icon screen coords the `tool` action taps (pencil/eraser/select/zoom) |
| `touch` | `/dev/input/eventN` | touchscreen device for the `tool` action |
| `plus` `minus` `note_shift` `note_erase` `note_play` `note_stop` `note_rec` `note_menu` `note_main` | hex | panel note numbers |

**Button actions:** `none` · `shift` · `erase` · `play` · `stop` · `rec` · `menu` · `main` · `tool`
(`tool` = cycle the Grid tools by tapping their on-screen icons).

## Changing a setting over SSH

Edit by hand — `ssh root@<device-ip>` then `vi /data/mpc-addins/midimouse/midimouse.conf` — or change one value in
place from your computer (replace `<device-ip>`):

```
# put Erase back on right-click
ssh root@<device-ip> "sed -i 's/^btn_right.*/btn_right erase/' /data/mpc-addins/midimouse/midimouse.conf"

# soften the wheel acceleration (1 = off)
ssh root@<device-ip> "sed -i 's/^wheel_accel.*/wheel_accel 2/' /data/mpc-addins/midimouse/midimouse.conf"

# invert the wheel direction
ssh root@<device-ip> "sed -i 's/^invert.*/invert 1/' /data/mpc-addins/midimouse/midimouse.conf"

# make the front side button record instead of play
ssh root@<device-ip> "sed -i 's/^btn_side_fwd.*/btn_side_fwd rec/' /data/mpc-addins/midimouse/midimouse.conf"

# Shift held-only instead of latched
ssh root@<device-ip> "sed -i 's/^shift_mode.*/shift_mode hold/' /data/mpc-addins/midimouse/midimouse.conf"
```

Give it ~2 seconds after any edit, then move the mouse — the change is live.

## Tool-icon coordinates on other models

`tool1`..`tool4` were captured on an MPC Live II. On a different screen size the taps may land off. To recapture,
read the screen while tapping each icon (`touchcap` in the repo's `lab/mouse/`) and set the new `x,y` values.
