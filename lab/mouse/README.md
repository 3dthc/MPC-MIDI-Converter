# Mouse control (via the panel MIDI stream)

MPC reads the panel (buttons, data wheel, Q-Links) as ALSA rawmidi on a "Private" port (buttons = note-on ch1, pads =
ch10, data wheel = a ch1 CC). This is the same stream the community `hwremap` shim (mmiroshnikov, MIT) rewrites to
remap buttons and inject touchscreen taps. So the panel CAN be driven from inside MPC, cleanly, by queuing MIDI bytes.

Plan:
- **Step C1 (capture, `midicap.c`):** read-only. Hook `snd_rawmidi_read` on the Private port and log the exact bytes
  the data wheel / Shift / Erase / buttons produce on *this* MPC Live.
- **Step M (build):** a mouse module (fork of hwremap, MIT, credited) that reads the USB mouse and queues: scroll ->
  data-wheel CC, a held mouse button -> Shift, right-click -> Erase.

`mouseenh.c` here is an earlier evdev-only experiment (wheel -> touch-drag scroll), kept for reference; the MIDI route
above is the real one.

## Step C1
Build on the PC, copy over, load read-only (reversible; reboot or `off` clears it):
```bash
docker run --rm --platform linux/arm/v7 -v "$PWD":/b -w /b arm32v7/gcc:11-bullseye gcc -shared -fPIC -O2 -o midicap.so midicap.c -ldl
scp midicap.so midicap.sh root@<MPC IP>:/tmp/
```
On the MPC (save project first): `sh /tmp/midicap.sh on`, re-SSH, `sh /tmp/midicap.sh status`, operate the controls,
then `sh /tmp/midicap.sh show`.
