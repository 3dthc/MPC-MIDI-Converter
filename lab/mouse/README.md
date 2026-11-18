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

`midicap.c` (Step C1) confirmed the bytes on this MPC Live II: Shift `90 31`, Erase `90 09`, `-` `90 37`, `+` `90 36`,
Private port `hw:1,0,1`. The data wheel itself never reaches a readable port (only a ~1/sec heartbeat SysEx), so `+`/`-`
is the value-stepping substitute.

## Step C1
Build on the PC, copy over, load read-only (reversible; reboot or `off` clears it):
```bash
docker run --rm --platform linux/arm/v7 -v "$PWD":/b -w /b arm32v7/gcc:11-bullseye gcc -shared -fPIC -O2 -o midicap.so midicap.c -ldl
scp midicap.so midicap.sh root@<MPC IP>:/tmp/
```
On the MPC (save project first): `sh /tmp/midicap.sh on`, re-SSH, `sh /tmp/midicap.sh status`, operate the controls,
then `sh /tmp/midicap.sh show`.

## Step M (mousemidi) — the mouse addin

`mousemidi.c`: a focused fork of hwremap's injection core (MIT, mmiroshnikov). It does **not** grab the mouse, so the
pointer and left-click stay native. It adds a second reader of the mouse (`/dev/input/event2`) that watches the wheel
and right/middle buttons — which MPC ignores — and queues panel MIDI into the Private rawmidi input:

| mouse action          | injected        | effect in MPC            |
|-----------------------|-----------------|--------------------------|
| scroll wheel up/down  | `+` / `-` tap   | step the selected value / scroll lists |
| hold middle + scroll  | Shift + `+`/`-` | note-length edit         |
| right-click           | Erase           | erase                    |

Optional `/data/mousemidi.conf` (key value per line) overrides defaults without a rebuild:
`mouse /dev/input/event2`, `minus 0x37`, `plus 0x36`, `shift 0x31`, `erase 0x09`, `invert 0`, `cc 0`
(`cc 1` injects a jog CC `B0 64` instead of `+`/`-`), `shift_btn middle|right`, `erase_btn right|middle`.

Build (armhf, Gen1/Gen2) and load (reversible — `off` or a reboot clears it):
```bash
arm-linux-gnueabihf-gcc -shared -fPIC -O2 -o mousemidi.so mousemidi.c -lpthread -ldl   # or the arm32v7/gcc docker one-liner above
scp mousemidi.so mousemidi.sh root@<MPC IP>:/tmp/
```
On the MPC (save your project first): `sh /tmp/mousemidi.sh on`, re-SSH to the new IP, `sh /tmp/mousemidi.sh status`,
then open a sequence and try the wheel. `sh /tmp/mousemidi.sh off` restores the stock MPC.
