// SPDX-License-Identifier: GPL-3.0-or-later
// Freestanding launcher (no libc): installed as <runtime>/bin/<name>, it executes
//   <runtime>/lib/<loader> --library-path <runtime libs> <runtime>/usr/bin/<name> args...
// The runtime's executables name an absolute ELF interpreter (/lib/ld-musl-*.so.1) that does not exist
// on the TV; ART finds dex2oat on $PATH, so this launcher stands in for it there.
#if defined(__aarch64__)
#define LOADER "ld-musl-aarch64.so.1"
static long sys4(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n, x0 __asm__("x0") = a, x1 __asm__("x1") = b, x2 __asm__("x2") = c,
                  x3 __asm__("x3") = d;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
enum { SYS_readlinkat = 78, SYS_execve = 221, SYS_exit = 93, SYS_write = 64 };
__asm__(".global _start\n_start:\n mov x0, sp\n bl cstart\n");
#elif defined(__x86_64__)
#define LOADER "ld-musl-x86_64.so.1"
static long sys4(long n, long a, long b, long c, long d) {
    long r;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "rcx", "r11", "memory");
    return r;
}
enum { SYS_readlinkat = 267, SYS_execve = 59, SYS_exit = 60, SYS_write = 1 };
__asm__(".global _start\n_start:\n mov %rsp, %rdi\n and $-16, %rsp\n call cstart\n");
#elif defined(__arm__)
// armv7 (the legacy runtime, docs/legacy-build.md): EABI syscalls, number in r7. Built with -marm so r7
// is not Thumb's frame pointer.
#define LOADER "ld-musl-armhf.so.1"
static long sys4(long n, long a, long b, long c, long d) {
    register long r7 __asm__("r7") = n, r0 __asm__("r0") = a, r1 __asm__("r1") = b, r2 __asm__("r2") = c,
                  r3 __asm__("r3") = d;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}
enum { SYS_readlinkat = 332, SYS_execve = 11, SYS_exit = 1, SYS_write = 4 };
__asm__(".global _start\n_start:\n mov r0, sp\n bic sp, sp, #7\n bl cstart\n");
#else
#error unsupported architecture
#endif

#define sys3(n, a, b, c) sys4((n), (a), (b), (c), 0)
#define AT_FDCWD (-100)
#define MAX_PATH 4096
#define MAX_ARGS 1024

static unsigned long len(const char *s) { unsigned long n = 0; while (s[n]) n++; return n; }

static char *cat(char *dst, const char *src) { while (*src) *dst++ = *src++; *dst = 0; return dst; }

__attribute__((noreturn)) static void die(const char *msg) {
    sys3(SYS_write, 2, (long)msg, (long)len(msg));
    sys3(SYS_exit, 127, 0, 0);
    for (;;) {}
}

static char self[MAX_PATH], root[MAX_PATH], loader[MAX_PATH], libs[4 * MAX_PATH], target[MAX_PATH];
static char *args[MAX_ARGS + 4];

__attribute__((used, noreturn)) void cstart(long *sp) {
    long argc = sp[0];
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    long n = sys4(SYS_readlinkat, AT_FDCWD, (long)"/proc/self/exe", (long)self, MAX_PATH - 1);
    if (n <= 0 || n >= MAX_PATH - 1) die("relaunch: cannot read /proc/self/exe\n");
    self[n] = 0;
    // self = <root>/bin/<name>
    char *name = self + n;
    while (name > self && name[-1] != '/') name--;
    unsigned long dir_len = (unsigned long)(name - self) - 1;  // <root>/bin
    if (dir_len < 4 || 4 * len(self) > sizeof(libs)) die("relaunch: unexpected location\n");
    for (unsigned long i = 0; i < dir_len - 4; i++) root[i] = self[i];
    root[dir_len - 4] = 0;
    cat(cat(cat(loader, root), "/lib/"), LOADER);
    char *p = libs;
    p = cat(cat(p, root), "/usr/lib:");
    p = cat(cat(p, root), "/usr/lib/art:");
    p = cat(cat(p, root), "/usr/lib/java/dex/art/natives:");
    cat(cat(p, root), "/lib");
    cat(cat(cat(target, root), "/usr/bin/"), name);
    if (argc > MAX_ARGS) die("relaunch: too many arguments\n");
    long k = 0;
    args[k++] = loader;
    args[k++] = "--library-path";
    args[k++] = libs;
    args[k++] = target;
    for (long i = 1; i < argc; i++) args[k++] = argv[i];
    args[k] = 0;
    sys3(SYS_execve, (long)loader, (long)args, (long)envp);
    die("relaunch: execve of the runtime loader failed\n");
}
