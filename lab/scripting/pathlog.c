/* pathlog: a diagnostic preload for MPC. Logs every file or folder MPC opens or looks up whose path mentions device
 * scripts (Devices, .ini, .lua, script) to /tmp/mpc-pathlog.txt, so we can see where MPC looks for them.
 * Load it for one run only (lab/scripting/pathlog.sh); remove it afterwards. */
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void note(const char *what, const char *path) {
    if (!path) return;
    if (!strstr(path, "Devices") && !strstr(path, ".ini") && !strstr(path, ".lua") && !strcasestr(path, "script")) return;
    char line[1200];
    int n = snprintf(line, sizeof line, "%s %s\n", what, path);
    static int (*ro)(const char *, int, ...);
    if (!ro) ro = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    int fd = ro("/tmp/mpc-pathlog.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd >= 0) { if (write(fd, line, n > 0 && n < (int)sizeof line ? n : 0) < 0) {} close(fd); }
}

typedef int (*open_f)(const char *, int, ...);
typedef int (*openat_f)(int, const char *, int, ...);
typedef DIR *(*opendir_f)(const char *);
typedef FILE *(*fopen_f)(const char *, const char *);
typedef int (*access_f)(const char *, int);
typedef int (*stat_f)(const char *, struct stat64 *);
typedef int (*xstat_f)(int, const char *, struct stat64 *);
#define NEXT(T, name) static T real; if (!real) real = (T)dlsym(RTLD_NEXT, name)

int open(const char *p, int f, ...) { va_list a; va_start(a, f); mode_t m = va_arg(a, int); va_end(a);
    NEXT(open_f, "open"); note("open", p); return real(p, f, m); }
int open64(const char *p, int f, ...) { va_list a; va_start(a, f); mode_t m = va_arg(a, int); va_end(a);
    NEXT(open_f, "open64"); note("open", p); return real(p, f, m); }
int openat(int d, const char *p, int f, ...) { va_list a; va_start(a, f); mode_t m = va_arg(a, int); va_end(a);
    NEXT(openat_f, "openat"); note("openat", p); return real(d, p, f, m); }
int openat64(int d, const char *p, int f, ...) { va_list a; va_start(a, f); mode_t m = va_arg(a, int); va_end(a);
    NEXT(openat_f, "openat64"); note("openat", p); return real(d, p, f, m); }
DIR *opendir(const char *p) { NEXT(opendir_f, "opendir"); note("opendir", p); return real(p); }
FILE *fopen(const char *p, const char *m) { NEXT(fopen_f, "fopen"); note("fopen", p); return real(p, m); }
FILE *fopen64(const char *p, const char *m) { NEXT(fopen_f, "fopen64"); note("fopen", p); return real(p, m); }
int access(const char *p, int m) { NEXT(access_f, "access"); note("access", p); return real(p, m); }
int stat64(const char *p, struct stat64 *s) { NEXT(stat_f, "stat64"); note("stat", p); return real(p, s); }
int __xstat64(int v, const char *p, struct stat64 *s) { NEXT(xstat_f, "__xstat64"); note("stat", p); return real(v, p, s); }
int __lxstat64(int v, const char *p, struct stat64 *s) { NEXT(xstat_f, "__lxstat64"); note("stat", p); return real(v, p, s); }
