/* MPC MIDI Converter: Standard MIDI File -> .mpcpattern (see convert.h). MIT licence.
 *
 * .mpcpattern is MPC's JSON note pattern (checked against patterns from MPC OS 3.10 expansions, 2026-11-17):
 *   {"pattern": {"length": 9223372036854775807, "events": [ ... ]}}
 *   note:      {"type": 2, "time": T, "len": L, "1": note, "2": velocity 0..1, "3": 0, "mod": 0, "modVal": 0.5..,
 *               "prob": 100, "ratchet": 1}
 *   pressure:  {"type": 1, "time": T, "len": 0, "1": 131, "2": amount 0..1, "3": note, "mod": 0, "modVal": 0.0}
 *   time and len in ticks at 960 per beat. On a drum track the note is the pad, 36 = A01. No tempo is stored. */
#include "convert.h"
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MPC_PPQ 960
#define MAX_FILE (16 * 1024 * 1024)
#define MAX_PARTS 64
#define MAX_EVENTS 400000   /* notes + pressure: far beyond any real MIDI file, and caps the work memory at ~20 MB */

typedef struct { int64_t start, len; unsigned char note, vel, ch; short part; } note_t;
typedef struct { int64_t time; unsigned char note, val, ch; short part; } press_t;

typedef struct {
    note_t *n; int nn, cap_n;
    press_t *p; int np, cap_p;
    int ppq, fmt;
    double bpm;
    char names[MAX_PARTS][48];
    int nparts;
} song_t;

static int push_note(song_t *s, note_t v) {
    if (s->nn + s->np >= MAX_EVENTS) return -1;
    if (s->nn == s->cap_n) {
        int c = s->cap_n ? s->cap_n * 2 : 1024;
        note_t *t = realloc(s->n, sizeof *t * c);
        if (!t) return -1;
        s->n = t; s->cap_n = c;
    }
    s->n[s->nn++] = v;
    return 0;
}

static int push_press(song_t *s, press_t v) {
    if (s->nn + s->np >= MAX_EVENTS) return -1;
    if (s->np == s->cap_p) {
        int c = s->cap_p ? s->cap_p * 2 : 1024;
        press_t *t = realloc(s->p, sizeof *t * c);
        if (!t) return -1;
        s->p = t; s->cap_p = c;
    }
    s->p[s->np++] = v;
    return 0;
}

static unsigned be(const unsigned char *b, int n) { unsigned v = 0; while (n--) v = v << 8 | *b++; return v; }

static int vlq(const unsigned char *b, long len, long *i, long *out) {
    long v = 0;
    for (int k = 0; k < 4; k++) {
        if (*i >= len) return -1;
        unsigned char c = b[(*i)++];
        v = v << 7 | (c & 0x7F);
        if (!(c & 0x80)) { *out = v; return 0; }
    }
    return -1;
}

/* Open notes per channel and key: a small stack so overlapping same-key notes pair first-in first-out. */
#define OPEN_DEPTH 8
typedef struct { int64_t t[OPEN_DEPTH]; unsigned char v[OPEN_DEPTH]; int n; } open_t;

static int close_note(song_t *s, open_t *o, int ch, int key, int64_t tick, int part) {
    if (!o->n) return 0;
    note_t nt = {o->t[0], tick - o->t[0], (unsigned char)key, o->v[0], (unsigned char)ch, (short)part};
    if (nt.len < 1) nt.len = 1;
    memmove(o->t, o->t + 1, sizeof(int64_t) * (o->n - 1));
    memmove(o->v, o->v + 1, o->n - 1);
    o->n--;
    return push_note(s, nt);
}

static int parse_track(song_t *s, const unsigned char *b, long len, int trk, open_t (*open)[128]) {
    long i = 0;
    int64_t tick = 0;
    int status = 0;
    memset(open, 0, sizeof(open_t) * 16 * 128);
    while (i < len) {
        long d;
        if (vlq(b, len, &i, &d)) return -1;
        tick += d;
        if (i >= len) return -1;
        if (b[i] & 0x80) status = b[i++];
        else if (!status) return -1;
        if (status == 0xFF) {
            long l;
            if (i >= len) return -1;
            int type = b[i++];
            if (vlq(b, len, &i, &l) || i + l > len) return -1;
            if (type == 0x51 && l == 3 && s->bpm == 0) {
                unsigned us = be(b + i, 3);
                if (us) s->bpm = 60000000.0 / us;
            } else if (type == 0x03 && trk < MAX_PARTS && !s->names[trk][0]) {
                int n = l < 47 ? (int)l : 47;
                memcpy(s->names[trk], b + i, n);
                s->names[trk][n] = 0;
            } else if (type == 0x2F) {
                break;
            }
            i += l;
            status = 0;   /* running status is cancelled by meta and sysex events */
            continue;
        }
        if (status == 0xF0 || status == 0xF7) {
            long l;
            if (vlq(b, len, &i, &l) || i + l > len) return -1;
            i += l;
            status = 0;
            continue;
        }
        int hi = status & 0xF0, ch = status & 0x0F;
        int nb = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
        if (i + nb > len) return -1;
        int a = b[i] & 0x7F, v = nb > 1 ? b[i + 1] & 0x7F : 0;
        i += nb;
        int part = trk < MAX_PARTS ? trk : MAX_PARTS - 1;
        if (hi == 0x90 && v > 0) {
            open_t *o = &open[ch][a];
            if (o->n == OPEN_DEPTH && close_note(s, o, ch, a, tick, part)) return -2;
            o->t[o->n] = tick; o->v[o->n] = (unsigned char)v; o->n++;
        } else if (hi == 0x80 || hi == 0x90) {
            if (close_note(s, &open[ch][a], ch, a, tick, part)) return -2;
        } else if (hi == 0xA0) {
            press_t p = {tick, (unsigned char)a, (unsigned char)v, (unsigned char)ch, (short)part};
            if (push_press(s, p)) return -2;
        }
    }
    for (int ch = 0; ch < 16; ch++)   /* notes still held at the end of the track end there */
        for (int k = 0; k < 128; k++)
            while (open[ch][k].n)
                if (close_note(s, &open[ch][k], ch, k, tick, trk < MAX_PARTS ? trk : MAX_PARTS - 1)) return -2;
    return 0;
}

static int parse(song_t *s, const unsigned char *d, long len, char *err, int en) {
    if (len < 14 || memcmp(d, "MThd", 4)) { snprintf(err, en, "not a MIDI file"); return -1; }
    unsigned hl = be(d + 4, 4);
    if (hl < 6 || hl > (unsigned)(len - 8)) { snprintf(err, en, "bad MIDI header"); return -1; }
    s->fmt = (int)be(d + 8, 2);
    int ntrk = (int)be(d + 10, 2);
    unsigned div = be(d + 12, 2);
    if (div & 0x8000) { snprintf(err, en, "SMPTE timing not supported"); return -1; }
    if (!div) { snprintf(err, en, "bad MIDI header"); return -1; }
    s->ppq = (int)div;
    open_t (*open)[128] = malloc(sizeof(open_t) * 16 * 128);
    if (!open) { snprintf(err, en, "out of memory"); return -1; }
    long pos = 8 + (long)hl;
    int t = 0, rc = 0;
    while (t < ntrk && pos + 8 <= len) {
        unsigned ucl = be(d + pos + 4, 4);
        long cl = ucl > (unsigned)(len - pos - 8) ? len - pos - 8 : (long)ucl;   /* a truncated last chunk: read what is there */
        if (!memcmp(d + pos, "MTrk", 4)) {
            rc = parse_track(s, d + pos + 8, cl, t, open);
            if (rc == -2) break;
            rc = 0;   /* a damaged track keeps the notes read before the damage */
            t++;
        }
        pos += 8 + cl;
    }
    free(open);
    if (rc == -2) { snprintf(err, en, s->nn + s->np >= MAX_EVENTS ? "too many notes" : "out of memory"); return -1; }
    s->nparts = t < MAX_PARTS ? t : MAX_PARTS;
    if (!s->nn) { snprintf(err, en, "no notes in file"); return -1; }
    return 0;
}

/* General MIDI drum note -> MPC pad index (0 = A01). The pad layout is in the README. */
static int gm_pad(int n) {
    switch (n) {
    case 35: case 36: return 0;                       /* A01 kick */
    case 38: case 40: return 1;                       /* A02 snare */
    case 42: return 2;                                /* A03 closed hat */
    case 46: return 3;                                /* A04 open hat */
    case 39: return 4;                                /* A05 clap */
    case 37: return 5;                                /* A06 side stick / rim */
    case 41: case 43: case 45: return 6;              /* A07 low tom */
    case 47: case 48: return 7;                       /* A08 mid tom */
    case 50: return 8;                                /* A09 high tom */
    case 49: case 52: case 55: case 57: return 9;     /* A10 crash / china / splash */
    case 51: case 53: case 59: return 10;             /* A11 ride / bell */
    case 44: return 11;                               /* A12 pedal hat */
    case 54: return 12;                               /* A13 tambourine */
    case 69: case 70: case 82: return 13;             /* A14 shaker / cabasa / maracas */
    case 56: return 14;                               /* A15 cowbell */
    default: return 15;                               /* A16 other percussion */
    }
}

typedef struct { int64_t time, len; int type, note; double val; } ev_t;

static int ev_cmp(const void *a, const void *b) {
    const ev_t *x = a, *y = b;
    if (x->time != y->time) return x->time < y->time ? -1 : 1;
    if (x->type != y->type) return x->type < y->type ? -1 : 1;   /* pressure before notes, as MPC writes them */
    return x->note - y->note;
}

static void safe_name(char *dst, int n, const char *src) {
    int j = 0;
    for (int i = 0; src[i] && j < n - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 32 || strchr("/\\:*?\"<>|", c)) c = '_';
        dst[j++] = (char)c;
    }
    while (j && (dst[j - 1] == ' ' || dst[j - 1] == '.')) j--;
    dst[j] = 0;
}

/* Writes the events of one part (part = -1: all) and returns the note count, 0 if it has none, -1 on error. */
static int write_part(const song_t *s, int part, int use_ch, const char *path, const conv_opts_t *o) {
    double k = (double)MPC_PPQ / s->ppq;
    int cap = s->nn + s->np, ne = 0, notes = 0;
    ev_t *ev = malloc(sizeof *ev * (cap ? cap : 1));
    if (!ev) return -1;
    int64_t first = -1;
    for (int i = 0; i < s->nn; i++) {
        const note_t *n = &s->n[i];
        if (part >= 0 && (use_ch ? n->ch : n->part) != part) continue;
        int64_t t = (int64_t)(n->start * k + 0.5);
        if (o->quantize > 0) t = (t + o->quantize / 2) / o->quantize * o->quantize;
        int nt = o->drum_map ? 36 + gm_pad(n->note) : n->note + o->transpose;
        if (nt < 0 || nt > 127) continue;
        int64_t l = (int64_t)(n->len * k + 0.5);
        ev[ne++] = (ev_t){t, l < 1 ? 1 : l, 2, nt, n->vel / 127.0};
        if (first < 0 || t < first) first = t;
        notes++;
    }
    if (!notes) { free(ev); return 0; }
    if (!o->drum_map)   /* pressure follows its note; drum-mapped pads drop it (several notes share a pad) */
        for (int i = 0; i < s->np; i++) {
            const press_t *p = &s->p[i];
            if (part >= 0 && (use_ch ? p->ch : p->part) != part) continue;
            int nt = p->note + o->transpose;
            if (nt < 0 || nt > 127) continue;
            ev[ne++] = (ev_t){(int64_t)(p->time * k + 0.5), 0, 1, nt, p->val / 127.0};
        }
    int64_t off = o->trim && first > 0 ? first / (MPC_PPQ * 4) * (MPC_PPQ * 4) : 0;
    qsort(ev, ne, sizeof *ev, ev_cmp);

    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.part", path);
    unlink(tmp);   /* a leftover from an interrupted run */
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);   /* never write through a planted link */
    FILE *f = fd >= 0 ? fdopen(fd, "w") : NULL;
    if (!f) { if (fd >= 0) close(fd); free(ev); return -1; }
    fprintf(f, "{\n    \"pattern\": {\n        \"length\": 9223372036854775807,\n        \"events\": [");
    int w = 0;
    for (int i = 0; i < ne; i++) {
        int64_t t = ev[i].time - off;
        if (t < 0) continue;
        fprintf(f, "%s\n            {\n                \"type\": %d,\n                \"time\": %" PRId64 ",\n"
                   "                \"len\": %" PRId64 ",\n", w++ ? "," : "", ev[i].type, t, ev[i].len);
        if (ev[i].type == 2)
            fprintf(f, "                \"1\": %d,\n                \"2\": %.6f,\n                \"3\": 0,\n"
                       "                \"mod\": 0,\n                \"modVal\": 0.500030517578125,\n"
                       "                \"prob\": 100,\n                \"ratchet\": 1\n            }",
                    ev[i].note, ev[i].val);
        else
            fprintf(f, "                \"1\": 131,\n                \"2\": %.6f,\n                \"3\": %d,\n"
                       "                \"mod\": 0,\n                \"modVal\": 0.0\n            }",
                    ev[i].val, ev[i].note);
    }
    fprintf(f, "\n        ]\n    }\n}\n");
    int bad = ferror(f);
    bad |= fclose(f);
    free(ev);
    if (bad || rename(tmp, path)) { remove(tmp); return -1; }
    return notes;
}

int mmc_is_midi(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return 0;
    char e[6] = {0};
    for (int i = 0; i < 5 && dot[i + 1]; i++) e[i] = (char)tolower((unsigned char)dot[i + 1]);
    return !strcmp(e, "mid") || !strcmp(e, "midi");
}

int mmc_convert_file(const char *midi_path, const char *out_dir, const conv_opts_t *o, char *err, int en) {
    int ifd = open(midi_path, O_RDONLY | O_NOFOLLOW);   /* a regular file only: no links, devices or fifos */
    struct stat ist;
    if (ifd < 0 || fstat(ifd, &ist) || !S_ISREG(ist.st_mode)) {
        if (ifd >= 0) close(ifd);
        snprintf(err, en, "can't open file");
        return -1;
    }
    FILE *f = fdopen(ifd, "rb");
    if (!f) { close(ifd); snprintf(err, en, "can't open file"); return -1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > MAX_FILE) { fclose(f); snprintf(err, en, len > 0 ? "file too large" : "empty file"); return -1; }
    unsigned char *d = malloc(len);
    if (!d || fread(d, 1, len, f) != (size_t)len) { fclose(f); free(d); snprintf(err, en, "can't read file"); return -1; }
    fclose(f);

    song_t *s = calloc(1, sizeof *s);
    if (!s) { free(d); snprintf(err, en, "out of memory"); return -1; }
    int rc = parse(s, d, len, err, en);
    free(d);
    if (rc) { free(s->n); free(s->p); free(s); return -1; }

    const char *slash = strrchr(midi_path, '/');
    char base[256], stem[256];
    snprintf(stem, sizeof stem, "%s", slash ? slash + 1 : midi_path);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = 0;
    safe_name(base, sizeof base, stem);
    if (!base[0]) snprintf(base, sizeof base, "pattern");
    char bpm[24] = "";
    if (o->bpm_in_name && s->bpm > 0) {
        double r = (double)(long)(s->bpm * 100 + 0.5) / 100;
        if (r == (long)r) snprintf(bpm, sizeof bpm, " (%ld BPM)", (long)r);
        else snprintf(bpm, sizeof bpm, " (%.2f BPM)", r);
    }

    /* Split by track; a format-0 file (one track) splits by channel instead. */
    int use_ch = s->fmt == 0 || s->nparts <= 1;
    int parts[MAX_PARTS], np = 0;
    if (o->split) {
        int seen[MAX_PARTS] = {0};
        for (int i = 0; i < s->nn; i++) {
            int p = use_ch ? s->n[i].ch : s->n[i].part;
            if (p >= 0 && p < MAX_PARTS && !seen[p]) seen[p] = 1;
        }
        for (int p = 0; p < MAX_PARTS; p++) if (seen[p]) parts[np++] = p;
    }
    int written = 0;
    char path[1024];
    if (np > 1) {
        char used[MAX_PARTS][64];
        for (int i = 0; i < np; i++) {
            char label[64], pn[64];
            if (!use_ch && s->names[parts[i]][0]) snprintf(label, sizeof label, "%s", s->names[parts[i]]);
            else if (use_ch) snprintf(label, sizeof label, "Ch %d", parts[i] + 1);
            else snprintf(label, sizeof label, "Track %d", parts[i] + 1);
            safe_name(pn, sizeof pn, label);
            if (!pn[0]) snprintf(pn, sizeof pn, "Part");
            /* two tracks with the same name must not overwrite each other's pattern */
            int dup = 1;
            for (int j = 0; j < i; j++) if (!strcmp(used[j], pn)) dup++;
            snprintf(used[i], sizeof used[i], "%s", pn);
            if (dup > 1) { size_t l = strlen(pn); snprintf(pn + (l < 54 ? l : 54), sizeof pn - (l < 54 ? l : 54), " %d", dup); }
            if (snprintf(path, sizeof path, "%s/%s - %s%s.mpcpattern", out_dir, base, pn, bpm) >= (int)sizeof path) {
                snprintf(err, en, "path too long"); written = -1; break;
            }
            int r = write_part(s, parts[i], use_ch, path, o);
            if (r < 0) { snprintf(err, en, "can't write pattern"); written = -1; break; }
            if (r > 0) written++;
        }
    } else {
        int r = snprintf(path, sizeof path, "%s/%s%s.mpcpattern", out_dir, base, bpm) >= (int)sizeof path ? -2
                : write_part(s, -1, 0, path, o);
        if (r == -2) { snprintf(err, en, "path too long"); written = -1; }
        else if (r < 0) { snprintf(err, en, "can't write pattern"); written = -1; }
        else if (r == 0) { snprintf(err, en, "no notes in range"); written = -1; }
        else written = 1;
    }
    if (written == 0) { snprintf(err, en, "no notes in range"); written = -1; }
    free(s->n); free(s->p); free(s);
    return written;
}
