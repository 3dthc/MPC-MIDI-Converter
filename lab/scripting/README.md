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
