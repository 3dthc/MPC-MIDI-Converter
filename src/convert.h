/* MPC MIDI Converter: Standard MIDI File (.mid/.midi) -> Akai .mpcpattern.
 * Plain C, no allocations on any audio path (called from a worker thread). MIT licence. */
#pragma once

typedef struct {
    int split;       /* 0: all tracks/channels in one pattern; 1: one pattern per track (per channel in a format-0 file) */
    int drum_map;    /* 1: General MIDI drum notes -> MPC pads (pattern note 36 = pad A01) */
    int quantize;    /* grid in MPC ticks (960 per beat), 0 = off */
    int transpose;   /* semitones, -48..48; not applied to drum-mapped notes */
    int trim;        /* 1: drop whole empty bars before the first note */
    int bpm_in_name; /* 1: add " (NNN BPM)" to the file name when the MIDI file has a tempo */
} conv_opts_t;

/* Converts one MIDI file. Writes <out_dir>/<base>[ - part][ (BPM)].mpcpattern and returns the number of
 * patterns written (>= 1), or -1 with a short reason in err. out_dir must exist. */
int mmc_convert_file(const char *midi_path, const char *out_dir, const conv_opts_t *o, char *err, int err_len);

/* 1 if the name ends in .mid or .midi (any case). */
int mmc_is_midi(const char *name);
