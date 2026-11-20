# MIDIMouse 1.0.0

MIDIMouse turns a USB mouse into a hardware-style editor for Akai MPC standalone devices. It runs as an Open MPC
addin, reads your mouse directly, and injects the matching MPC panel controls — no extra hardware, and it loads
automatically on boot.

## What it does
- **Wheel** — steps the selected value / note length, with acceleration (a fast spin jumps further)
- **Middle button** — latches Shift (tap on, tap off), so you can left-click the on-screen Shift options
- **Right-click** — cycles the Grid tools: pencil → eraser → select → zoom
- **Rear side button** — Stop · **Front side button** — Play
- **Left-click and pointer** stay native (select as normal)

## Highlights
- Auto-detects any mouse, hot-plug included, so you can swap mice freely
- Works with 3-button and 5-button mice (side-button actions just don't fire on a 3-button)
- Fully configurable live, no restart (see below)
- Standard Open MPC addin: permanent across reboots, with a clean uninstall

## Compatibility
- Two builds: **armv7** (32-bit, glibc 2.17) for Gen 1 on MPC OS 2.x and 3.x; **aarch64** for Gen 2 on OS 3.x.
- Verified on an MPC Live II (Gen 1, OS 3). The OS 2 and Gen 2 builds are correct by construction but untested on
  that hardware. Panel notes and the tool-icon coordinates were captured on the Live II — on a different screen
  size the right-click tool-cycle coords (`tool1..tool4`) may need nudging. Everything is tunable live.

## Configuring (over SSH)
Settings live in `/data/mpc-addins/midimouse/midimouse.conf`. **Edits apply live within ~2 seconds — no restart,
no reinstall.** Edit by hand (`ssh root@<device-ip>` then `vi` the file), or change one value from your computer:

```sh
# put Erase back on right-click
ssh root@<device-ip> "sed -i 's/^btn_right.*/btn_right erase/' /data/mpc-addins/midimouse/midimouse.conf"

# soften the wheel acceleration (1 = off, higher = jumpier)
ssh root@<device-ip> "sed -i 's/^wheel_accel.*/wheel_accel 2/' /data/mpc-addins/midimouse/midimouse.conf"

# invert the wheel direction
ssh root@<device-ip> "sed -i 's/^invert.*/invert 1/' /data/mpc-addins/midimouse/midimouse.conf"

# make the front side button record instead of play
ssh root@<device-ip> "sed -i 's/^btn_side_fwd.*/btn_side_fwd rec/' /data/mpc-addins/midimouse/midimouse.conf"

# Shift held-only instead of latched
ssh root@<device-ip> "sed -i 's/^shift_mode.*/shift_mode hold/' /data/mpc-addins/midimouse/midimouse.conf"
```

**Any button** (`btn_middle`, `btn_right`, `btn_side_back`, `btn_side_fwd`) takes an action:
`none` · `shift` · `erase` · `play` · `stop` · `rec` · `menu` · `main` · `tool` (cycle Grid tools).
Other keys: `mouse` (auto / a device), `shift_mode` (latch/hold), `shift_timeout`, `wheel_accel`, `invert`,
`tool1..tool4` (tool-icon coords), `note_*` (panel notes). Full reference in `SETTINGS.md` inside the zip.

## Install
Unzip, copy the folder to the device, run the installer (it restarts MPC — save your project first):
```sh
scp -r MIDIMouse-1.0.0 root@<device-ip>:/tmp/
ssh root@<device-ip> sh /tmp/MIDIMouse-1.0.0/install.sh
```
Requires root/SSH access (a modded unit). Uninstall: `sh /data/mpc-addins/midimouse/uninstall.sh`.
