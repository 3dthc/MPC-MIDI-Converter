/* MPC MIDI Converter: the plugin. A file browser on the MPC's drives that turns .mid/.midi files into .mpcpattern
 * files (src/convert.c), one at a time or a whole folder at once. An audio effect that passes its input through
 * untouched, so it can sit on any track. All disk work runs on one worker thread; set_param only posts a command,
 * get_param only copies text under a short lock. MIT licence. */
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "engine.h"
#include "convert.h"

#ifndef MMC_ROOT
#define MMC_ROOT "/media"        /* MPC drives are mounted here; the top of the browser */
#endif
#define ROWS 8
#define MAX_ENT 4000
#define BATCH_DEPTH 4             /* folder levels a batch goes into below the chosen folder */
#define OUT_DIR "MIDI Import"

static const int QUANT[] = {0, 960, 480, 320, 240, 160, 120};   /* Off, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 */
#define NQUANT 7

typedef struct { char name[256]; int dir; } ent_t;

enum { CMD_NONE, CMD_LIST, CMD_ENTER, CMD_UP, CMD_CONVERT, CMD_BATCH, CMD_QUIT };

typedef struct {
    pthread_t th;
    pthread_mutex_t mu;           /* guards everything below; never held during disk I/O */
    pthread_cond_t cv;
    int cmd; char cmd_arg[1024];
    int busy;

    char dir[1024];               /* folder shown */
    ent_t *ent; int nent;         /* its entries: folders first, then MIDI files */
    int page, sel;                /* sel: entry index of the chosen file, -1 none */
    char status[128];
    unsigned rev;

    int split, drums, quant, transpose, trim, save_next, bpm_name;
} st_t;

static void bump(st_t *s) { s->rev++; }

static void set_status(st_t *s, const char *msg) {
    pthread_mutex_lock(&s->mu);
    snprintf(s->status, sizeof s->status, "%s", msg);
    bump(s);
    pthread_mutex_unlock(&s->mu);
}

static int ent_cmp(const void *a, const void *b) {
    const ent_t *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return strcasecmp(x->name, y->name);
}

static int is_dir_at(const char *dir, const struct dirent *d) {
    if (d->d_type == DT_DIR) return 1;
    if (d->d_type != DT_UNKNOWN) return 0;   /* symlinks are not followed */
    char p[1300]; struct stat st;
    snprintf(p, sizeof p, "%s/%s", dir, d->d_name);
    return !lstat(p, &st) && S_ISDIR(st.st_mode);
}

/* System mounts under /media that hold no user files (settings, the read-only plugin store). */
static int hidden_mount(const char *name) {
    return !strcmp(name, "az01-internal") || !strncmp(name, "acvs", 4);
}

/* The name a drive shows as: the internal drive's mount has a system name. */
static const char *drive_label(const char *name) {
    return !strcmp(name, "az01-internal-sd") ? "Internal" : name;
}

/* The internal drive can be mounted twice (az01-internal-sd and a named mount of the same storage): keep one,
 * the named one. Same storage = same device and inode of the mount's root. */
static int drop_aliases(const char *dir, ent_t *e, int n) {
    struct stat st[64];
    int ok[64] = {0}, m = n < 64 ? n : 64;
    for (int i = 0; i < m; i++) {
        char p[1300];
        snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
        ok[i] = !stat(p, &st[i]);
    }
    int w = 0;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        if (i < m && ok[i] && !strcmp(e[i].name, "az01-internal-sd"))
            for (int j = 0; j < m; j++)
                if (j != i && ok[j] && st[j].st_dev == st[i].st_dev && st[j].st_ino == st[i].st_ino) dup = 1;
        if (!dup) e[w++] = e[i];
    }
    return w;
}

/* Lists `dir` into a fresh array (no lock held), then swaps it in. */
static int list_dir(st_t *s, const char *dir, const char *select) {
    DIR *dp = opendir(dir);
    if (!dp) return -1;
    ent_t *e = malloc(sizeof *e * MAX_ENT);
    if (!e) { closedir(dp); return -1; }
    int n = 0, top = !strcmp(dir, MMC_ROOT);
    struct dirent *d;
    while ((d = readdir(dp)) && n < MAX_ENT) {
        if (d->d_name[0] == '.') continue;
        if (top && hidden_mount(d->d_name)) continue;
        int isd = is_dir_at(dir, d);
        if (!isd && (top || !mmc_is_midi(d->d_name))) continue;
        snprintf(e[n].name, sizeof e[n].name, "%s", d->d_name);
        e[n].dir = isd;
        n++;
    }
    closedir(dp);
    if (top) n = drop_aliases(dir, e, n);
    qsort(e, n, sizeof *e, ent_cmp);
    pthread_mutex_lock(&s->mu);
    free(s->ent);
    s->ent = e; s->nent = n;
    snprintf(s->dir, sizeof s->dir, "%s", dir);
    s->page = 0; s->sel = -1;
    if (select)
        for (int i = 0; i < n; i++)
            if (!strcmp(e[i].name, select)) { s->page = i / ROWS; s->sel = e[i].dir ? -1 : i; break; }
    snprintf(s->status, sizeof s->status, n ? "Pick a MIDI file, or convert the whole folder" :
             top ? "No drives found" : "No MIDI files or folders here");
    bump(s);
    pthread_mutex_unlock(&s->mu);
    return 0;
}

static int mkdirs(const char *path) {
    char p[1024];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; *c; c++)
        if (*c == '/') { *c = 0; if (mkdir(p, 0755) && errno != EEXIST) return -1; *c = '/'; }
    return mkdir(p, 0755) && errno != EEXIST ? -1 : 0;
}

/* /media/<drive>/... -> /media/<drive> */
static void drive_root(const char *path, char *out, int n) {
    size_t r = strlen(MMC_ROOT);
    const char *p = path + r + (path[r] == '/');
    const char *slash = strchr(p, '/');
    int len = slash ? (int)(slash - path) : (int)strlen(path);
    snprintf(out, n, "%.*s", len, path);
}

static void opts_of(st_t *s, conv_opts_t *o, int *save_next) {
    pthread_mutex_lock(&s->mu);
    o->split = s->split; o->drum_map = s->drums; o->quantize = QUANT[s->quant];
    o->transpose = s->transpose; o->trim = s->trim; o->bpm_in_name = s->bpm_name;
    *save_next = s->save_next;
    pthread_mutex_unlock(&s->mu);
}

static void base_name(const char *path, char *out, int n) {
    const char *sl = strrchr(path, '/');
    snprintf(out, n, "%s", sl ? sl + 1 : path);
}

/* Output folder for a MIDI file in `src_dir`: next to it, or <drive>/MIDI Import[/<rel>]. */
static int out_dir_for(const char *src_dir, const char *rel, int save_next, char *out, int n) {
    if (save_next) snprintf(out, n, "%s", src_dir);
    else {
        char root[1024];
        drive_root(src_dir, root, sizeof root);
        if (rel && rel[0]) snprintf(out, n, "%s/" OUT_DIR "/%s", root, rel);
        else snprintf(out, n, "%s/" OUT_DIR, root);
    }
    return mkdirs(out);
}

static void do_convert(st_t *s, const char *dir, const char *name) {
    conv_opts_t o; int save_next;
    opts_of(s, &o, &save_next);
    char in[1300], out[1300], err[96] = "";
    snprintf(in, sizeof in, "%s/%s", dir, name);
    if (out_dir_for(dir, NULL, save_next, out, sizeof out)) { set_status(s, "Can't make the output folder"); return; }
    int r = mmc_convert_file(in, out, &o, err, sizeof err);
    if (r < 0) {
        char m[128];
        snprintf(m, sizeof m, "Failed: %s", err);
        set_status(s, m);
        return;
    }
    char where[64];
    snprintf(where, sizeof where, "%s", save_next ? "next to the file" : "in " OUT_DIR);
    pthread_mutex_lock(&s->mu);
    snprintf(s->status, sizeof s->status, "Done: %d pattern%s %s", r, r == 1 ? "" : "s", where);
    bump(s);
    pthread_mutex_unlock(&s->mu);
}

typedef struct { int files, patterns, failed; } tally_t;

static void batch_walk(st_t *s, const char *dir, const char *rel, int depth, const conv_opts_t *o, int save_next, tally_t *t) {
    DIR *dp = opendir(dir);
    if (!dp) return;
    struct dirent *d;
    /* read the names first: writing into this folder (save next to file) must not disturb the listing */
    int cap = 256, n = 0;
    ent_t *e = malloc(sizeof *e * cap);
    while (e && (d = readdir(dp))) {
        if (d->d_name[0] == '.') continue;
        int isd = is_dir_at(dir, d);
        if (!isd && !mmc_is_midi(d->d_name)) continue;
        if (isd && (depth >= BATCH_DEPTH || !strcmp(d->d_name, OUT_DIR))) continue;
        if (n == cap) { ent_t *g = realloc(e, sizeof *e * cap * 2); if (!g) break; e = g; cap *= 2; }
        snprintf(e[n].name, sizeof e[n].name, "%s", d->d_name);
        e[n++].dir = isd;
    }
    closedir(dp);
    if (!e) return;
    qsort(e, n, sizeof *e, ent_cmp);
    for (int i = 0; i < n; i++) {
        char path[1300], r2[1300];
        snprintf(path, sizeof path, "%s/%s", dir, e[i].name);
        if (e[i].dir) {
            if (rel[0]) snprintf(r2, sizeof r2, "%s/%s", rel, e[i].name);
            else snprintf(r2, sizeof r2, "%s", e[i].name);
            batch_walk(s, path, r2, depth + 1, o, save_next, t);
            continue;
        }
        char out[1300], err[96];
        t->files++;
        char m[128];
        snprintf(m, sizeof m, "Converting %d: %.100s", t->files, e[i].name);
        set_status(s, m);
        if (out_dir_for(dir, rel, save_next, out, sizeof out)) { t->failed++; continue; }
        int r = mmc_convert_file(path, out, o, err, sizeof err);
        if (r < 0) t->failed++;
        else t->patterns += r;
    }
    free(e);
}

static void do_batch(st_t *s, const char *dir) {
    conv_opts_t o; int save_next;
    opts_of(s, &o, &save_next);
    char top[256];
    base_name(dir, top, sizeof top);
    tally_t t = {0};
    /* MIDI Import/<folder name>/<subfolders>, so a batch keeps the pack's own structure */
    batch_walk(s, dir, save_next ? "" : top, 0, &o, save_next, &t);
    pthread_mutex_lock(&s->mu);
    if (!t.files) snprintf(s->status, sizeof s->status, "No MIDI files in this folder");
    else snprintf(s->status, sizeof s->status, "Done: %d file%s, %d pattern%s%s%d failed", t.files, t.files == 1 ? "" : "s",
                  t.patterns, t.patterns == 1 ? "" : "s", ", ", t.failed);
    if (t.files && !t.failed) { char *c = strstr(s->status, ", 0 failed"); if (c) *c = 0; }
    bump(s);
    pthread_mutex_unlock(&s->mu);
}

static void *worker(void *arg) {
    st_t *s = arg;
    for (;;) {
        pthread_mutex_lock(&s->mu);
        while (s->cmd == CMD_NONE) pthread_cond_wait(&s->cv, &s->mu);
        int cmd = s->cmd;
        char a[1024], dir[1024];
        snprintf(a, sizeof a, "%s", s->cmd_arg);
        snprintf(dir, sizeof dir, "%s", s->dir);
        s->cmd = CMD_NONE;
        s->busy = 1;
        pthread_mutex_unlock(&s->mu);
        if (cmd == CMD_QUIT) break;

        if (cmd == CMD_LIST) {
            /* a saved folder that is gone (card removed) falls back to the nearest folder that still exists */
            char d[1024];
            snprintf(d, sizeof d, "%s", a[0] ? a : dir);
            if (strncmp(d, MMC_ROOT, strlen(MMC_ROOT))) snprintf(d, sizeof d, "%s", MMC_ROOT);
            while (list_dir(s, d, NULL) && strcmp(d, MMC_ROOT)) {
                char *sl = strrchr(d, '/');
                if (!sl || sl == d) { snprintf(d, sizeof d, "%s", MMC_ROOT); continue; }
                *sl = 0;
                if (strlen(d) < strlen(MMC_ROOT)) snprintf(d, sizeof d, "%s", MMC_ROOT);
            }
        } else if (cmd == CMD_ENTER) {
            char d[1300];
            snprintf(d, sizeof d, "%s/%s", dir, a);
            if (list_dir(s, d, NULL)) set_status(s, "Can't open that folder");
        } else if (cmd == CMD_UP) {
            if (strcmp(dir, MMC_ROOT)) {
                char d[1024], from[256];
                snprintf(d, sizeof d, "%s", dir);
                char *sl = strrchr(d, '/');
                snprintf(from, sizeof from, "%s", sl ? sl + 1 : "");
                if (sl && sl != d) *sl = 0;
                if (strlen(d) < strlen(MMC_ROOT)) snprintf(d, sizeof d, "%s", MMC_ROOT);
                if (list_dir(s, d, from)) list_dir(s, MMC_ROOT, NULL);
            }
        } else if (cmd == CMD_CONVERT) {
            do_convert(s, dir, a);
        } else if (cmd == CMD_BATCH) {
            if (!strcmp(dir, MMC_ROOT)) set_status(s, "Open a drive or folder first");
            else do_batch(s, dir);
        }
        pthread_mutex_lock(&s->mu);
        s->busy = 0;
        bump(s);
        pthread_mutex_unlock(&s->mu);
    }
    return NULL;
}

/* Called with s->mu held. A command while the worker is busy is dropped (taps don't queue up behind a batch). */
static void post(st_t *s, int cmd, const char *arg) {
    if (s->busy || s->cmd != CMD_NONE) return;
    s->cmd = cmd;
    snprintf(s->cmd_arg, sizeof s->cmd_arg, "%s", arg ? arg : "");
    pthread_cond_signal(&s->cv);
}

static void *create(const char *data_dir) {
    (void)data_dir;
    st_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    pthread_mutex_init(&s->mu, NULL);
    pthread_cond_init(&s->cv, NULL);
    snprintf(s->dir, sizeof s->dir, "%s", MMC_ROOT);
    s->sel = -1;
    s->bpm_name = 1;
    snprintf(s->status, sizeof s->status, "Loading...");
    s->cmd = CMD_LIST;
    if (pthread_create(&s->th, NULL, worker, s)) {
        pthread_mutex_destroy(&s->mu); pthread_cond_destroy(&s->cv); free(s); return NULL;
    }
    return s;
}

static void destroy(void *inst) {
    st_t *s = inst;
    pthread_mutex_lock(&s->mu);
    s->cmd = CMD_QUIT;   /* the worker finishes the file it is on, then stops */
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->mu);
    pthread_join(s->th, NULL);
    pthread_mutex_destroy(&s->mu);
    pthread_cond_destroy(&s->cv);
    free(s->ent);
    free(s);
}

static void midi(void *i, const uint8_t *m, int n) { (void)i, (void)m, (void)n; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void set_param(void *inst, const char *key, const char *val) {
    st_t *s = inst;
    double v = atof(val);
    pthread_mutex_lock(&s->mu);
    if (!strncmp(key, "file_", 5) && key[5] >= '1' && key[5] <= '8' && !key[6]) {
        if (v > 0.5) {   /* a tap on a list row (the release and the host's echo come as 0) */
            int i = s->page * ROWS + (key[5] - '1');
            if (i < s->nent) {
                if (s->ent[i].dir) post(s, CMD_ENTER, s->ent[i].name);
                else if (!s->busy) {
                    s->sel = i;
                    snprintf(s->status, sizeof s->status, "Selected. Tap CONVERT FILE");
                }
            }
            bump(s);
        }
    } else if (!strcmp(key, "up")) { if (v > 0.5) post(s, CMD_UP, NULL); }
    else if (!strcmp(key, "prev")) { if (v > 0.5 && s->page > 0) { s->page--; bump(s); } }
    else if (!strcmp(key, "next")) { if (v > 0.5 && (s->page + 1) * ROWS < s->nent) { s->page++; bump(s); } }
    else if (!strcmp(key, "refresh")) { if (v > 0.5) post(s, CMD_LIST, s->dir); }
    else if (!strcmp(key, "convert")) {
        if (v > 0.5) {
            if (s->sel >= 0 && s->sel < s->nent) post(s, CMD_CONVERT, s->ent[s->sel].name);
            else { snprintf(s->status, sizeof s->status, "Pick a MIDI file first"); bump(s); }
        }
    } else if (!strcmp(key, "convert_all")) { if (v > 0.5) post(s, CMD_BATCH, NULL); }
    else if (!strcmp(key, "tracks")) s->split = clampi((int)v, 0, 1);
    else if (!strcmp(key, "drums")) s->drums = clampi((int)v, 0, 1);
    else if (!strcmp(key, "quantize")) s->quant = clampi((int)v, 0, NQUANT - 1);
    else if (!strcmp(key, "transpose")) s->transpose = clampi((int)(v < 0 ? v - 0.5 : v + 0.5), -24, 24);
    else if (!strcmp(key, "trim")) s->trim = clampi((int)v, 0, 1);
    else if (!strcmp(key, "save_to")) s->save_next = clampi((int)v, 0, 1);
    else if (!strcmp(key, "bpm_name")) s->bpm_name = clampi((int)v, 0, 1);
    else if (!strcmp(key, "state")) {
        int o[7];
        const char *dl = strstr(val, "\ndir=");
        if (sscanf(val, "opts=%d,%d,%d,%d,%d,%d,%d", &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6]) == 7) {
            s->split = clampi(o[0], 0, 1); s->drums = clampi(o[1], 0, 1); s->quant = clampi(o[2], 0, NQUANT - 1);
            s->transpose = clampi(o[3], -24, 24); s->trim = clampi(o[4], 0, 1);
            s->save_next = clampi(o[5], 0, 1); s->bpm_name = clampi(o[6], 0, 1);
        }
        if (dl) {
            char d[1024];
            snprintf(d, sizeof d, "%s", dl + 5);
            char *nl = strchr(d, '\n');
            if (nl) *nl = 0;
            /* a restore just after create() replaces the first listing */
            if (s->cmd == CMD_LIST || s->cmd == CMD_NONE) {
                s->cmd = CMD_LIST;
                snprintf(s->cmd_arg, sizeof s->cmd_arg, "%s", d);
                pthread_cond_signal(&s->cv);
            }
        }
        bump(s);
    }
    pthread_mutex_unlock(&s->mu);
}

static int get_param(void *inst, const char *key, char *b, int n) {
    st_t *s = inst;
    int r = 0;
    pthread_mutex_lock(&s->mu);
    if (!strcmp(key, "display_rev")) r = snprintf(b, n, "%u", s->rev);
    else if (!strcmp(key, "folder")) {
        const char *p = s->dir + strlen(MMC_ROOT);
        if (*p == '/') p++;
        int len = (int)strlen(p);
        if (!*p) r = snprintf(b, n, "Drives");
        else {
            char lab[1100];
            const char *sl = strchr(p, '/');
            char drv[256];
            snprintf(drv, sizeof drv, "%.*s", sl ? (int)(sl - p) : len, p);
            snprintf(lab, sizeof lab, "%s%s", drive_label(drv), sl ? sl : "");
            int ll = (int)strlen(lab);
            /* long paths keep their end, the part that tells folders apart */
            if (ll > 60) r = snprintf(b, n, "...%s", lab + ll - 57);
            else r = snprintf(b, n, "%s", lab);
        }
    } else if (!strcmp(key, "status")) r = snprintf(b, n, "%s", s->status);
    else if (!strcmp(key, "page")) {
        int pages = (s->nent + ROWS - 1) / ROWS;
        r = snprintf(b, n, "%d / %d", pages ? s->page + 1 : 0, pages);
    } else if (!strncmp(key, "file_", 5) && key[5] >= '1' && key[5] <= '8') {
        int i = s->page * ROWS + (key[5] - '1');
        if (!key[6]) {   /* the row's text: folders end in " /" */
            if (i < s->nent) {
                const char *nm = strcmp(s->dir, MMC_ROOT) ? s->ent[i].name : drive_label(s->ent[i].name);
                r = snprintf(b, n, "%s%s", nm, s->ent[i].dir ? "  /" : "");
            }
            else r = snprintf(b, n, " ");
        } else if (!strcmp(key + 6, "_on")) r = snprintf(b, n, "%d", i < s->nent && i == s->sel);
    } else if (!strcmp(key, "up") || !strcmp(key, "prev") || !strcmp(key, "next") || !strcmp(key, "refresh") ||
               !strcmp(key, "convert") || !strcmp(key, "convert_all")) r = snprintf(b, n, "0");
    else if (!strcmp(key, "tracks")) r = snprintf(b, n, "%d", s->split);
    else if (!strcmp(key, "drums")) r = snprintf(b, n, "%d", s->drums);
    else if (!strcmp(key, "quantize")) r = snprintf(b, n, "%d", s->quant);
    else if (!strcmp(key, "transpose")) r = snprintf(b, n, "%d", s->transpose);
    else if (!strcmp(key, "trim")) r = snprintf(b, n, "%d", s->trim);
    else if (!strcmp(key, "save_to")) r = snprintf(b, n, "%d", s->save_next);
    else if (!strcmp(key, "bpm_name")) r = snprintf(b, n, "%d", s->bpm_name);
    else if (!strcmp(key, "state"))
        r = snprintf(b, n, "opts=%d,%d,%d,%d,%d,%d,%d\ndir=%s\n", s->split, s->drums, s->quant, s->transpose, s->trim,
                     s->save_next, s->bpm_name, s->dir);
    pthread_mutex_unlock(&s->mu);
    if (r >= n) r = n - 1;
    return r;
}

static void render(void *inst, int16_t *out, int frames) { (void)inst; memset(out, 0, sizeof(int16_t) * 2 * frames); }

static void process(void *inst, const int16_t *in, int16_t *out, int frames) {
    (void)inst;
    if (out != in) memmove(out, in, sizeof(int16_t) * 2 * frames);   /* audio passes through untouched */
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, process, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
