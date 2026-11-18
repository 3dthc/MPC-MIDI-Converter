/* panelsniff: read-only lab preload. Logs the bytes MPC reads from its control-surface MIDI device
 * (/dev/snd/midiC1D*) and from the input event devices (/dev/input/event*), so we can see the exact
 * message the data wheel (and buttons/Q-Links) produce from inside MPC. Writes /tmp/mpc-panel.txt.
 * It injects nothing and changes nothing. Lab use only; remove after capturing (panelsniff.sh off / reboot). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXFD 4096
static char tag[MAXFD];                 /* 1 = a panel/input fd we log */

typedef int (*open_f)(const char *, int, ...);
typedef ssize_t (*read_f)(int, void *, size_t);

static int interesting(const char *p) {
    return p && (strstr(p, "/dev/snd/midiC1D") || strstr(p, "/dev/input/event"));
}

static void logbytes(int fd, const unsigned char *b, ssize_t n) {
    static open_f ro;
    if (!ro) ro = (open_f)dlsym(RTLD_NEXT, "open");
    char line[300];
    int k = snprintf(line, sizeof line, "fd%d:", fd);
    for (ssize_t i = 0; i < n && k < (int)sizeof line - 4; i++) k += snprintf(line + k, sizeof line - k, " %02x", b[i]);
    if (k < (int)sizeof line - 1) line[k++] = '\n';
    int lf = ro("/tmp/mpc-panel.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (lf >= 0) { if (write(lf, line, k) < 0) {} close(lf); }
}

static void maybe_tag(int fd, const char *path) {
    if (fd >= 0 && fd < MAXFD) tag[fd] = interesting(path) ? 1 : 0;
}

int open(const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static open_f real; if (!real) real = (open_f)dlsym(RTLD_NEXT, "open");
    int fd = real(p, f, m); maybe_tag(fd, p); return fd;
}
int open64(const char *p, int f, ...) {
    va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a);
    static open_f real; if (!real) real = (open_f)dlsym(RTLD_NEXT, "open64");
    int fd = real(p, f, m); maybe_tag(fd, p); return fd;
}

ssize_t read(int fd, void *buf, size_t n) {
    static read_f real; if (!real) real = (read_f)dlsym(RTLD_NEXT, "read");
    ssize_t r = real(fd, buf, n);
    if (r > 0 && fd >= 0 && fd < MAXFD && tag[fd]) logbytes(fd, buf, r);
    return r;
}
