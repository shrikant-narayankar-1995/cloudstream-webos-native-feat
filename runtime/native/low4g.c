// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * low4g.so: preloaded into the plugin runtime on aarch64 (runtime_launcher.cpp, --preload).
 *
 * ART's art::MemMap::MapInternalArtLow4GBAllocator() looks for free address space below 4 GB by probing it
 * page by page with msync() and starting over after every foreign mapping. On some LG TVs (seen on a
 * 43UR7500, webOS 25 / 10.3.1, kernel 5.4, 39-bit address space) that search never finishes: dalvikvm sits
 * at 100 % CPU before "ready" and the app shows "Loading plugins..." forever.
 *
 * This replacement reads /proc/self/maps, takes the first gap that fits (from 256 MB up) and maps it with
 * MAP_FIXED_NOREPLACE. libartbase calls the function directly (no PLT), so the constructor overwrites its
 * first 16 bytes with `ldr x16, #8; br x16; .quad replacement`. If the symbol is missing nothing changes.
 *
 * Built like the other helpers: clang, no sysroot; libc functions are declared here and resolved from the
 * runtime's own musl when the library is loaded.
 */
typedef unsigned long size_t;
typedef long ssize_t;
typedef long off_t;
typedef unsigned long uintptr_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern ssize_t read(int, void *, size_t);
extern int close(int);
extern void *mmap(void *, size_t, int, int, int, off_t);
extern int munmap(void *, size_t);
extern int mprotect(void *, size_t, int);
extern int *__errno_location(void);

#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define O_RDONLY 0
#define O_CLOEXEC 02000000
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_FIXED 0x10
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FAILED ((void *)-1)
#define ENOMEM 12
#define LOW_START 0x10000000UL
#define LIMIT (1UL << 32)
#define SYM "_ZN3art6MemMap29MapInternalArtLow4GBAllocatorEmiiil"

static void *try_at(uintptr_t at, size_t len, int prot, int flags, int fd, off_t off) {
    void *p = mmap((void *)at, len, prot, (flags & ~MAP_FIXED) | MAP_FIXED_NOREPLACE, fd, off);
    if (p == MAP_FAILED) return 0;
    if ((uintptr_t)p != at) {  // kernel without MAP_FIXED_NOREPLACE: it was only a hint
        munmap(p, len);
        return 0;
    }
    return p;
}

static int hex(const char **s, uintptr_t *v) {
    uintptr_t r = 0;
    int n = 0;
    for (;; (*s)++, n++) {
        char c = **s;
        if (c >= '0' && c <= '9') r = r * 16 + (uintptr_t)(c - '0');
        else if (c >= 'a' && c <= 'f') r = r * 16 + (uintptr_t)(c - 'a' + 10);
        else break;
    }
    *v = r;
    return n > 0;
}

// first gap of at least len bytes between LOW_START and 4 GB, or 0
static uintptr_t find_gap(size_t len) {
    static char buf[1 << 16];
    int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    uintptr_t prev_end = LOW_START, found = 0;
    char line[256];
    size_t ll = 0;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n && !found; i++) {
            if (buf[i] != '\n') {
                if (ll < sizeof line - 1) line[ll++] = buf[i];
                continue;
            }
            line[ll] = 0;
            ll = 0;
            const char *s = line;
            uintptr_t a, b;
            if (!hex(&s, &a) || *s++ != '-' || !hex(&s, &b)) continue;
            if (b <= prev_end) continue;
            if (a >= LIMIT) { i = n; break; }
            if (a > prev_end && a - prev_end >= len) found = prev_end;
            else prev_end = b;
        }
        if (found) break;
    }
    close(fd);
    if (!found && prev_end < LIMIT && LIMIT - prev_end >= len) found = prev_end;
    return found;
}

// static void* art::MemMap::MapInternalArtLow4GBAllocator(size_t, int, int, int, off_t)
static void *low4g_map(size_t length, int prot, int flags, int fd, off_t offset) {
    length = (length + 0xfff) & ~0xfffUL;
    for (int attempt = 0; attempt < 4; attempt++) {  // another thread may take the gap first
        uintptr_t at = find_gap(length);
        if (!at) break;
        void *p = try_at(at, length, prot, flags, fd, offset);
        if (p) return p;
    }
    *__errno_location() = ENOMEM;
    return MAP_FAILED;
}

__attribute__((constructor)) static void low4g_init(void) {
    void *h = dlopen("libartbase.so", RTLD_NOW | RTLD_GLOBAL);
    uint32_t *fn = h ? (uint32_t *)dlsym(h, SYM) : 0;
    if (!fn) return;
    uintptr_t pg = (uintptr_t)fn & ~0xfffUL;
    if (mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC)) return;
    uint64_t target = (uint64_t)(uintptr_t)low4g_map;
    fn[0] = 0x58000050;  // ldr x16, #8
    fn[1] = 0xd61f0200;  // br x16
    fn[2] = (uint32_t)target;
    fn[3] = (uint32_t)(target >> 32);
    // make the new instructions visible to instruction fetch (no __clear_cache in musl)
    __asm__ volatile("dc cvau, %0\n\tdsb ish\n\tic ivau, %0\n\tdsb ish\n\tisb" : : "r"(fn) : "memory");
    mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
}
