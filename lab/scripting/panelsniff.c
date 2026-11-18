/* panelsniff (v2): read-only lab preload. Logs which input/MIDI devices MPC opens and the bytes it reads from them,
 * so we can find where the data wheel / buttons / Q-Links come in and capture the exact message. Writes
 * /tmp/mpc-panel.txt. Injects nothing, changes nothing. Lab use only (panelsniff.sh off / reboot removes it). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXFD 8192
static char tag[MAXFD];

typedef int (*open_f)(const char *, int, ...);
typedef int (*openat_f)(int, const char *, int, ...);
typedef ssize_t (*read_f)(int, void *, size_t);

static int logfd(void) {
    static open_f ro;
    if (!ro) ro = (open_f)dlsym(RTLD_NEXT, "open");
    return ro("/tmp/mpc-panel.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
}
static void logline(const char *s, int len) { int f = logfd(); if (f >= 0) { if (write(f, s, len) < 0) {} close(f); } }

__attribute__((constructor)) static void hello(void) {
    char s[64]; int n = snprintf(s, sizeof s, "== sniffer loaded, pid %d\n", getpid()); logline(s, n);
}

/* devices worth watching: the control-surface MIDI, the external MIDI, and the input event nodes */
static int interesting(const char *p) {
    return p && (strstr(p, "/dev/snd/midiC1D") || strstr(p, "/dev/snd/seq") || strstr(p, "/dev/input/event") ||
                 strstr(p, "/dev/hidraw"));
}
static void tag_fd(int fd, const char *path) {
    if (fd < 0 || fd >= MAXFD) return;
    tag[fd] = interesting(path) ? 1 : 0;
    if (tag[fd]) { char s[300]; int n = snprintf(s, sizeof s, "open fd%d %s\n", fd, path); logline(s, n); }
}

int open(const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static open_f r; if (!r) r = (open_f)dlsym(RTLD_NEXT, "open");
    int fd = r(p, f, m); tag_fd(fd, p); return fd;
}
int open64(const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static open_f r; if (!r) r = (open_f)dlsym(RTLD_NEXT, "open64");
    int fd = r(p, f, m); tag_fd(fd, p); return fd;
}
int openat(int d, const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static openat_f r; if (!r) r = (openat_f)dlsym(RTLD_NEXT, "openat");
    int fd = r(d, p, f, m); tag_fd(fd, p); return fd;
}
int openat64(int d, const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static openat_f r; if (!r) r = (openat_f)dlsym(RTLD_NEXT, "openat64");
    int fd = r(d, p, f, m); tag_fd(fd, p); return fd;
}

ssize_t read(int fd, void *buf, size_t n) {
    static read_f r; if (!r) r = (read_f)dlsym(RTLD_NEXT, "read");
    ssize_t got = r(fd, buf, n);
    if (got > 0 && fd >= 0 && fd < MAXFD && tag[fd]) {
        const unsigned char *b = buf;
        char s[300]; int k = snprintf(s, sizeof s, "fd%d:", fd);
        for (ssize_t i = 0; i < got && k < (int)sizeof s - 4; i++) k += snprintf(s + k, sizeof s - k, " %02x", b[i]);
        if (k < (int)sizeof s - 1) s[k++] = '\n';
        logline(s, k);
    }
    return got;
}
