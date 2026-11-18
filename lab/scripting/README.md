# Scripting lab

Experiments toward importing MIDI straight into a track (with tempo) and mouse helpers, using the device-script
interface built into MPC OS 3.10.1 (Lua; the same interface as Akai's MPK mini scripts). Not part of the plugin.

Step 1 (`lab.sh`, `mmcLab.lua`): find out whether MPC loads a user device script, and from where.

Step 1b (`pathlog.c`, `pathlog.sh`): if no popup appears, find where MPC looks for device scripts. Build on the PC
(Ubuntu, Docker), copy to the MPC, then run `pathlog.sh` there:

```bash
docker run --rm --platform linux/arm/v7 -v "$PWD":/b -w /b arm32v7/gcc:11-bullseye gcc -shared -fPIC -O2 -o pathlog.so pathlog.c -ldl
scp pathlog.so pathlog.sh root@<MPC IP>:/tmp/
```

## Mouse control (priority)

The panel (data wheel, buttons, Q-Links) is read by MPC directly from `/dev/snd/midiC1D0`, which MPC holds open, so
it cannot be fed from outside MPC. The route is an addin loaded into MPC (like `nova-audio`/`remote`) that reads the
mouse and injects the panel's own events into MPC's read of that device. No script signature is involved.

Step M1 (`panelsniff.c`, `panelsniff.sh`): read-only. Confirm the data wheel comes through `midiC1D0` and capture its
exact bytes from inside MPC. Build on the PC, copy over, run on the MPC:

```bash
docker run --rm --platform linux/arm/v7 -v "$PWD":/b -w /b arm32v7/gcc:11-bullseye gcc -shared -fPIC -O2 -o panelsniff.so panelsniff.c -ldl
scp panelsniff.so panelsniff.sh root@<MPC IP>:/tmp/
```
