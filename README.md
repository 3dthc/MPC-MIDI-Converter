# MPC MIDI Converter

A free, open-source tool that turns MIDI files (`.mid`, `.midi`) into MPC pattern files (`.mpcpattern`), right on the MPC.
Load a pattern onto any track and the notes land in that track, at your project's tempo, without MPC's MIDI import making a new
sequence.

This is the **MPC plugin**. Windows (`.exe`) and Mac (`.dmg`) versions are planned.

## Using it

1. Add **MPC MIDI Converter** to any track as an insert effect. It passes audio through untouched.
2. Browse the drives in the list: tap a folder to open it, **UP** to go back.
3. Tap a MIDI file to pick it, then **CONVERT FILE**. Or tap **CONVERT FOLDER** to convert every MIDI file in the folder you
   are in, and in its subfolders.
4. Find the patterns in the **MIDI Import** folder at the root of the same drive (or next to the MIDI files, see SAVE TO).
   Load one onto a track from the browser.

## Options

| Option | What it does |
|---|---|
| **Tracks** | **Merge** puts the whole file in one pattern. **Split** makes one pattern per MIDI track (per channel, for single-track files), named after the track. |
| **Drum map** | **GM to Pads** moves General MIDI drum notes onto MPC pads (layout below), for drum loops made in other software. |
| **Quantize** | Snaps note starts to the grid you pick. |
| **Transpose** | Shifts notes up or down, ±24 semitones (not applied to drum-mapped notes). |
| **Trim start** | Removes empty bars before the first note. |
| **Save to** | **MIDI Import** (a folder at the root of the drive; a folder conversion keeps the pack's own folder structure) or **Next to File**. |

Patterns do not store tempo, so the file's tempo is added to the pattern's name, for example `Groove 01 (92 BPM).mpcpattern`.

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
