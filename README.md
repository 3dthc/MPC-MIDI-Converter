# MPC MIDI Converter

A free, open-source tool that turns MIDI files (`.mid`, `.midi`) into MPC pattern files (`.mpcpattern`), right on the MPC.
Load a pattern onto any track and the notes land in that track, at your project's tempo, without MPC's MIDI import making a new
sequence.

This is the **MPC plugin**. Windows (`.exe`) and Mac (`.dmg`) versions are planned.

## Using it

1. Add **MPC MIDI Converter** to a track (it is listed with the instruments).
2. Tap a drive, then tap folders to open them. **BACK** goes up a folder; drag the scroll bar beside the list to scroll.
3. Tap a MIDI file to pick it (it lights up), then tap it again or tap **CONVERT FILE**. **CONVERT FOLDER** converts
   every MIDI file in the folder you are in, and in its subfolders.
4. Find the patterns in the **MIDI Converts** folder at the top level of the drive your MIDI files are on (the status
   line says where). Load one onto a track from MPC's browser.

**HOW TO** shows these steps on the plugin's page.

## Options

| Option | What it does |
|---|---|
| **Split tracks** | **Off** puts the whole file in one pattern. **On** makes one pattern per MIDI track (per channel, for single-track files), named after the track. |
| **Drum map** | **On** moves General MIDI drum notes onto MPC pads (layout below), for drum loops made in other software. |
| **Quantize** | Snaps note starts to 1/8, 1/16 or 1/32. |
| **List** | **Large** shows 5 big rows; **Compact** shows 8. |

Patterns do not store tempo, so the file's tempo is added to the pattern's name, for example `Groove 01 (92 BPM).mpcpattern`.
A folder conversion keeps the pack's own folder structure inside MIDI Converts.

### Drum map (GM to Pads)

| Pad | Sound | Pad | Sound |
|---|---|---|---|
| A01 | Kick | A09 | High tom |
| A02 | Snare | A10 | Crash, splash, china |
| A03 | Closed hat | A11 | Ride, ride bell |
| A04 | Open hat | A12 | Pedal hat |
| A05 | Clap | A13 | Tambourine |
| A06 | Side stick, rim | A14 | Shaker, cabasa, maracas |
| A07 | Low tom | A15 | Cowbell |
| A08 | Mid tom | A16 | Any other percussion |

## Building

Needs Docker and [Open MPC](https://github.com/open-mpc/mpc-vst-plugins) checked out next to this folder.

```sh
./build.sh          # tests, then builds vst/build/<.so> and the skin
test/run_tests.sh   # just the tests (converter, 300 damaged files, the plugin's browser), no Docker
```

## Licence

MIT. If this tool helps you, you can support it at [ko-fi.com/3dthc](https://ko-fi.com/3dthc).
