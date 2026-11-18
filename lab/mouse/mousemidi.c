/* mousemidi: let a USB mouse drive MPC standalone editing, cleanly.
 *
 * MPC reads the panel as ALSA rawmidi on a "Private" port and already reads the USB mouse for the pointer and
 * left-click (as touch). The mouse's WHEEL and RIGHT/MIDDLE buttons are ignored by MPC. This addin reads the mouse
 * as a second reader (no grab, so pointer/left-click are untouched) and injects panel MIDI into MPC's Private input:
 *   wheel        -> + / - buttons (step the selected value / scroll lists; captured notes 0x36 / 0x37 on an MPC Live)
 *   middle       -> Shift (latch by default: tap on, tap off; or set shift_mode hold)      (Shift note 0x31)
 *   right click  -> Erase                                                                 (Erase note 0x09)
 * Latch exists because MPC maps all mouse buttons to one touch contact: holding a button blocks left-click, so a
 * held-Shift + left-click combo can't get through. Latching Shift leaves only the left button pressed at click time.
 * Buttons are note-on on channel 1 (0x90 note 0x7F press / 0x00 release). Overridable in /data/mousemidi.conf.
 *
 * The rawmidi injection (hook snd_rawmidi_open/read + poll with a wake pipe, queue bytes for MPC to read) follows
 * hwremap by mmiroshnikov (MIT, open-mpc/mpc-vst-plugins tools/mpc_patch/hwremap). This file is MIT too. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct _snd_rawmidi snd_rawmidi_t;
typedef struct _snd_rawmidi_info snd_rawmidi_info_t;

#define QSIZE 4096
static int (*real_open)(snd_rawmidi_t **, snd_rawmidi_t **, const char *, int);
static ssize_t (*real_read)(snd_rawmidi_t *, void *, size_t);
static snd_rawmidi_t *priv_in;
static int priv_fd = -1, wake_fd[2] = {-1, -1};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char q[QSIZE];
static int qhead, qtail;

/* config (overridable via /data/mousemidi.conf: key value per line) */
static char mouse_dev[128] = "/dev/input/event2";
static int NOTE_PLUS = 0x36, NOTE_MINUS = 0x37, NOTE_SHIFT = 0x31, NOTE_ERASE = 0x09;
static int invert = 0, use_cc = 0;         /* use_cc 1: inject jog CC B0 64 01/7F instead of +/- buttons */
static int shift_btn = BTN_MIDDLE, erase_btn = BTN_RIGHT;
static int shift_latch = 1;                /* 1 latch (tap on/off), 0 hold-while-pressed */
static int shift_timeout = 12;             /* latch auto-release after this many idle seconds (0 = never) */
static int shift_on = 0;                   /* current latched Shift state */

static void *next_sym(const char *name) {
    static void *(*libc_dlsym)(void *, const char *);
    if (!libc_dlsym) {
        static const char *const vers[] = {"2.34", "2.17", "2.4", "2.2.5", "2.0"};
        for (size_t i = 0; i < sizeof vers / sizeof *vers && !libc_dlsym; i++)
            libc_dlsym = (void *(*)(void *, const char *))dlvsym(RTLD_DEFAULT, "dlsym", vers[i]);
    }
    return (libc_dlsym ? libc_dlsym : dlsym)(RTLD_NEXT, name);
}
static void init_syms(void) {
    if (!real_open) real_open = next_sym("snd_rawmidi_open");
    if (!real_read) real_read = next_sym("snd_rawmidi_read");
}
static void push(unsigned char b) { int n = (qtail + 1) % QSIZE; if (n == qhead) return; q[qtail] = b; qtail = n; }
static void wake_reader(void) { if (wake_fd[1] >= 0) { char c = 1; if (write(wake_fd[1], &c, 1) < 0) {} } }
static void q3(unsigned char a, unsigned char b, unsigned char c) { pthread_mutex_lock(&lock); push(a); push(b); push(c); pthread_mutex_unlock(&lock); wake_reader(); }
static void tap(int note) { q3(0x90, note, 0x7F); q3(0x90, note, 0x00); }

static int parse_int(const char *s) {   /* 0x.. hex or plain decimal; no libc strtol (avoids a C23-only symbol) */
    int sign = 1, v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        for (s += 2; *s; s++) {
            int d = (*s >= '0' && *s <= '9') ? *s - '0'
                  : (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10
                  : (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1;
            if (d < 0) break;
            v = v * 16 + d;
        }
    } else {
        for (; *s >= '0' && *s <= '9'; s++) v = v * 10 + (*s - '0');
    }
    return v * sign;
}
static void load_conf(void) {
    /* the packaged addin keeps its conf in its own folder; /data/mousemidi.conf is the lab-loader fallback */
    FILE *f = fopen("/data/mpc-addins/mousemidi/mousemidi.conf", "r");
    if (!f) f = fopen("/data/mousemidi.conf", "r");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        char k[64] = "", v[64] = "";
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        int n = 0; while (p[n] && p[n] != ' ' && p[n] != '\t' && p[n] != '\n' && n < 63) { k[n] = p[n]; n++; }
        k[n] = 0; p += n;
        while (*p == ' ' || *p == '\t') p++;
        n = 0; while (p[n] && p[n] != ' ' && p[n] != '\t' && p[n] != '\n' && n < 63) { v[n] = p[n]; n++; }
        v[n] = 0;
        if (!k[0] || !v[0] || k[0] == '#') continue;
        if (!strcmp(k, "mouse")) snprintf(mouse_dev, sizeof mouse_dev, "%s", v);
        else if (!strcmp(k, "plus")) NOTE_PLUS = parse_int(v);
        else if (!strcmp(k, "minus")) NOTE_MINUS = parse_int(v);
        else if (!strcmp(k, "shift")) NOTE_SHIFT = parse_int(v);
        else if (!strcmp(k, "erase")) NOTE_ERASE = parse_int(v);
        else if (!strcmp(k, "invert")) invert = parse_int(v);
        else if (!strcmp(k, "cc")) use_cc = parse_int(v);
        else if (!strcmp(k, "shift_btn")) shift_btn = !strcmp(v, "right") ? BTN_RIGHT : BTN_MIDDLE;
        else if (!strcmp(k, "erase_btn")) erase_btn = !strcmp(v, "middle") ? BTN_MIDDLE : BTN_RIGHT;
        else if (!strcmp(k, "shift_mode")) shift_latch = !strcmp(v, "hold") ? 0 : 1;
        else if (!strcmp(k, "shift_timeout")) shift_timeout = parse_int(v);
    }
    fclose(f);
}

static void step(int dir) {   /* dir +1 up / -1 down (after invert) */
    if (invert) dir = -dir;
    if (use_cc) q3(0xB0, 0x64, dir > 0 ? 0x01 : 0x7F);   /* jog CC */
    else tap(dir > 0 ? NOTE_PLUS : NOTE_MINUS);
}

static void shift_set(int on) {   /* latch: assert/release Shift, only on a real change */
    if (on == shift_on) return;
    shift_on = on;
    q3(0x90, NOTE_SHIFT, on ? 0x7F : 0x00);
}

static void *mouse_thread(void *arg) {
    (void)arg;
    int fd = -1;
    time_t last = 0;
    for (;;) {
        if (fd < 0) { fd = open(mouse_dev, O_RDONLY); if (fd < 0) { sleep(2); continue; } }
        fd_set r; FD_ZERO(&r); FD_SET(fd, &r);
        struct timeval tv = {1, 0};   /* tick once a second only while a latch could time out */
        int sel = select(fd + 1, &r, NULL, NULL,
                         (shift_latch && shift_on && shift_timeout > 0) ? &tv : NULL);
        if (sel == 0) {   /* timed out with Shift latched and idle -> release it */
            if (last && time(NULL) - last >= shift_timeout) shift_set(0);
            continue;
        }
        if (sel < 0) continue;
        struct input_event e;
        ssize_t n = read(fd, &e, sizeof e);
        if (n != (ssize_t)sizeof e) { close(fd); fd = -1; sleep(1); continue; }
        last = time(NULL);
        if (e.type == EV_REL && e.code == REL_WHEEL) {
            int c = e.value < 0 ? -e.value : e.value, d = e.value > 0 ? 1 : -1;
            for (int i = 0; i < c && i < 8; i++) step(d);
        } else if (e.type == EV_KEY && e.code == shift_btn) {
            if (shift_latch) { if (e.value == 1) shift_set(!shift_on); }   /* tap toggles */
            else q3(0x90, NOTE_SHIFT, e.value ? 0x7F : 0x00);             /* hold while pressed */
        } else if (e.type == EV_KEY && e.code == erase_btn && e.value == 1) {
            tap(NOTE_ERASE);
        }
    }
    return NULL;
}

static int is_private(snd_rawmidi_t *h) {
    size_t (*info_sizeof)(void) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info_sizeof");
    int (*info)(snd_rawmidi_t *, snd_rawmidi_info_t *) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info");
    const char *(*subname)(const snd_rawmidi_info_t *) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info_get_subdevice_name");
    int priv = 0;
    if (info_sizeof && info) {
        snd_rawmidi_info_t *ri = calloc(1, info_sizeof());
        if (ri && info(h, ri) == 0) { const char *sn = subname ? subname(ri) : NULL; if (sn && strstr(sn, "Private")) priv = 1; }
        free(ri);
    }
    return priv;
}

__attribute__((visibility("default")))
int snd_rawmidi_open(snd_rawmidi_t **in, snd_rawmidi_t **out, const char *name, int mode) {
    init_syms();
    int r = real_open(in, out, name, mode);
    if (r == 0 && in && *in && !priv_in && is_private(*in)) {
        priv_in = *in;
        int (*pdesc)(snd_rawmidi_t *, struct pollfd *, unsigned int) = dlsym(RTLD_DEFAULT, "snd_rawmidi_poll_descriptors");
        struct pollfd pfd;
        priv_fd = (pdesc && pdesc(*in, &pfd, 1) == 1) ? pfd.fd : -1;
        if (wake_fd[0] < 0 && pipe2(wake_fd, O_NONBLOCK | O_CLOEXEC) != 0) wake_fd[0] = wake_fd[1] = -1;
        load_conf();
        pthread_t t; pthread_create(&t, NULL, mouse_thread, NULL); pthread_detach(t);
    }
    return r;
}

__attribute__((visibility("default")))
ssize_t snd_rawmidi_read(snd_rawmidi_t *h, void *buffer, size_t size) {
    init_syms();
    if (h != priv_in || size == 0) return real_read(h, buffer, size);
    unsigned char *out = buffer;
    size_t n = 0;
    pthread_mutex_lock(&lock);
    while (n < size && qhead != qtail) { out[n++] = q[qhead]; qhead = (qhead + 1) % QSIZE; }
    pthread_mutex_unlock(&lock);
    if (n > 0) return (ssize_t)n;      /* hand MPC our injected bytes first */
    return real_read(h, buffer, size); /* otherwise the real panel input */
}

__attribute__((visibility("default")))
int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    static int (*real_poll)(struct pollfd *, nfds_t, int);
    if (!real_poll) real_poll = next_sym("poll");
    int idx = -1;
    if (priv_fd >= 0 && wake_fd[0] >= 0 && nfds < 16)
        for (nfds_t i = 0; i < nfds; i++) if (fds[i].fd == priv_fd && (fds[i].events & POLLIN)) idx = (int)i;
    if (idx < 0) return real_poll(fds, nfds, timeout);
    pthread_mutex_lock(&lock);
    int queued = qhead != qtail;
    pthread_mutex_unlock(&lock);
    if (queued) { for (nfds_t i = 0; i < nfds; i++) fds[i].revents = 0; fds[idx].revents = POLLIN; return 1; }
    struct pollfd all[16];
    memcpy(all, fds, nfds * sizeof *fds);
    all[nfds].fd = wake_fd[0]; all[nfds].events = POLLIN; all[nfds].revents = 0;
    int r = real_poll(all, nfds + 1, timeout);
    if (r < 0) return r;
    for (nfds_t i = 0; i < nfds; i++) fds[i].revents = all[i].revents;
    if (all[nfds].revents) { char d[64]; while (read(wake_fd[0], d, sizeof d) > 0) {} r--; if (!fds[idx].revents) { fds[idx].revents = POLLIN; r++; } }
    return r;
}
__attribute__((visibility("default")))
int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, size_t len) { (void)len; return poll(fds, nfds, timeout); }
