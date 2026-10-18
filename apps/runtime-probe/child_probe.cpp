// SPDX-License-Identifier: GPL-3.0-or-later
// Platform checks for a second, on-device runtime process (CloudStream feasibility gate).
// Everything here runs inside a child process exec'd by the app, so results describe what a runtime
// child would get from the webOS jail. No plugin code is involved.
#include "child_probe.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace csprobe {
namespace {

void emit(const char *key, const std::string &value) { std::printf("%s=%s\n", key, value.c_str()); }

std::string err(const char *what) { return std::string("FAIL ") + what + ": " + std::strerror(errno); }

// A function returning 42, in the instruction set of this process.
#if defined(__arm__)
const unsigned char kStub[] = {0x2a, 0x00, 0xa0, 0xe3,   // mov r0, #42   (ARM state)
                               0x1e, 0xff, 0x2f, 0xe1};  // bx lr
#elif defined(__x86_64__) || defined(__i386__)
const unsigned char kStub[] = {0xb8, 0x2a, 0x00, 0x00, 0x00, 0xc3};  // mov eax, 42; ret
#elif defined(__aarch64__)
const unsigned char kStub[] = {0x40, 0x05, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6};  // mov w0, #42; ret
#endif

int call_stub(void *code) {
    __builtin___clear_cache(static_cast<char *>(code), static_cast<char *>(code) + sizeof(kStub));
    return reinterpret_cast<int (*)()>(code)();
}

// JIT style 1: one RWX mapping.
std::string jit_rwx() {
    void *p = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return err("mmap rwx");
    std::memcpy(p, kStub, sizeof(kStub));
    const int r = call_stub(p);
    munmap(p, 4096);
    return r == 42 ? "ok" : "FAIL stub returned " + std::to_string(r);
}

// JIT style 2: write, then mprotect to RX (W^X).
std::string jit_wx() {
    void *p = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return err("mmap rw");
    std::memcpy(p, kStub, sizeof(kStub));
    if (mprotect(p, 4096, PROT_READ | PROT_EXEC) != 0) {
        std::string e = err("mprotect rx");
        munmap(p, 4096);
        return e;
    }
    const int r = call_stub(p);
    munmap(p, 4096);
    return r == 42 ? "ok" : "FAIL stub returned " + std::to_string(r);
}

// AOT style: code in a file on a writable filesystem, mapped executable (how ART maps .oat/.odex).
std::string file_exec_map(const std::string &dir) {
    const std::string path = dir + "/csprobe-stub.bin";
    int fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return err("create");
    std::vector<unsigned char> page(4096, 0);
    std::memcpy(page.data(), kStub, sizeof(kStub));
    const bool wrote = write(fd, page.data(), page.size()) == static_cast<ssize_t>(page.size());
    void *p = wrote ? mmap(nullptr, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0) : MAP_FAILED;
    std::string out;
    if (!wrote) {
        out = err("write");
    } else if (p == MAP_FAILED) {
        out = err("mmap r-x file");
    } else {
        const int r = call_stub(p);
        out = r == 42 ? "ok" : "FAIL stub returned " + std::to_string(r);
        munmap(p, 4096);
    }
    close(fd);
    unlink(path.c_str());
    return out;
}

std::string entropy() {
    unsigned char buf[16];
#ifdef SYS_getrandom
    const long n = syscall(SYS_getrandom, buf, sizeof(buf), 0);
    std::string out = n == static_cast<long>(sizeof(buf)) ? "getrandom ok" : err("getrandom");
#else
    std::string out = "getrandom not in headers";
#endif
    for (const char *dev : {"/dev/urandom", "/dev/random"}) {
        int fd = open(dev, O_RDONLY | O_CLOEXEC);
        out += std::string("; ") + dev + (fd >= 0 && read(fd, buf, 4) == 4 ? " ok" : " " + err("open/read"));
        if (fd >= 0) close(fd);
    }
    return out;
}

std::string rlimits() {
    std::string out;
    const struct {
        const char *name;
        int res;
    } lims[] = {{"AS", RLIMIT_AS},       {"DATA", RLIMIT_DATA},     {"STACK", RLIMIT_STACK},
                {"NOFILE", RLIMIT_NOFILE}, {"NPROC", RLIMIT_NPROC}, {"MEMLOCK", RLIMIT_MEMLOCK}};
    for (const auto &l : lims) {
        struct rlimit r {};
        if (getrlimit(l.res, &r) != 0) continue;
        auto fmt = [](rlim_t v) { return v == RLIM_INFINITY ? std::string("inf") : std::to_string(v); };
        out += std::string(out.empty() ? "" : " ") + l.name + "=" + fmt(r.rlim_cur) + "/" + fmt(r.rlim_max);
    }
    return out;
}

std::string read_matching(const char *path, const std::vector<std::string> &prefixes) {
    std::ifstream in(path);
    std::string line, out;
    while (std::getline(in, line)) {
        for (const auto &p : prefixes) {
            if (line.compare(0, p.size(), p) == 0) out += (out.empty() ? "" : " | ") + line;
        }
    }
    return out.empty() ? "unavailable" : out;
}

// Mount options for the filesystems a runtime would use.
std::string mounts(const std::string &app_dir, const std::string &tmp_dir) {
    std::ifstream in("/proc/self/mounts");
    std::string dev, mp, fs, opts, out;
    int a, b;
    std::vector<std::pair<std::string, std::string>> all;
    while (in >> dev >> mp >> fs >> opts >> a >> b) all.emplace_back(mp, fs + " " + opts);
    for (const std::string &want : {std::string("/tmp"), std::string("/dev/shm"), app_dir, tmp_dir}) {
        // Longest mount point that is a prefix of the path.
        const std::pair<std::string, std::string> *best = nullptr;
        for (const auto &m : all) {
            const bool prefix = want.compare(0, m.first.size(), m.first) == 0 &&
                                (want.size() == m.first.size() || m.first == "/" || want[m.first.size()] == '/');
            if (prefix && (!best || m.first.size() >= best->first.size())) best = &m;
        }
        out += (out.empty() ? "" : " | ") + want + " -> " + (best ? best->first + " " + best->second : "?");
    }
    return out;
}

void *thread_body(void *) { return reinterpret_cast<void *>(7); }

std::string threads(int n) {
    std::vector<pthread_t> ts(n);
    int made = 0;
    for (; made < n; ++made) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 1024 * 1024);  // ART's default managed stack is about this size
        const int rc = pthread_create(&ts[made], &attr, thread_body, nullptr);
        pthread_attr_destroy(&attr);
        if (rc != 0) break;
    }
    for (int i = 0; i < made; ++i) pthread_join(ts[i], nullptr);
    return std::to_string(made) + "/" + std::to_string(n) + " created";
}

// Name resolution and a TCP connection from the child (the runtime does its own HTTP).
std::string tcp_connect(const char *host, const char *port) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    const int g = getaddrinfo(host, port, &hints, &res);
    if (g != 0) return std::string("FAIL getaddrinfo: ") + gai_strerror(g);
    std::string out = "FAIL no address connected";
    for (auto *ai = res; ai; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        struct timeval tv {5, 0};
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            out = std::string("ok ") + (ai->ai_family == AF_INET6 ? "ipv6" : "ipv4");
            close(fd);
            break;
        }
        out = err("connect");
        close(fd);
    }
    freeaddrinfo(res);
    return out;
}

}  // namespace

int run_child_probe(const std::string &app_dir, const std::string &tmp_dir, bool network) {
    struct utsname u {};
    uname(&u);
    emit("child.pid", std::to_string(getpid()));
    emit("child.ids", "uid=" + std::to_string(getuid()) + " gid=" + std::to_string(getgid()) +
                          " euid=" + std::to_string(geteuid()));
    emit("child.uname", std::string(u.sysname) + " " + u.release + " " + u.machine);
    emit("child.ptr_bits", std::to_string(sizeof(void *) * 8));
    emit("child.mounts", mounts(app_dir, tmp_dir));
    emit("child.jit_rwx", jit_rwx());
    emit("child.jit_wx", jit_wx());
    emit("child.file_exec_map_tmp", file_exec_map(tmp_dir));
    emit("child.file_exec_map_shm", file_exec_map("/dev/shm"));
    emit("child.entropy", entropy());
    emit("child.rlimits", rlimits());
    emit("child.meminfo", read_matching("/proc/meminfo", {"MemTotal", "MemAvailable", "CommitLimit", "Committed_AS"}));
    emit("child.overcommit", read_matching("/proc/sys/vm/overcommit_memory", {""}));
    emit("child.cpu", read_matching("/proc/cpuinfo", {"Features", "CPU architecture"}));
    emit("child.ncpu", std::to_string(sysconf(_SC_NPROCESSORS_ONLN)));
    struct statvfs sv {};
    emit("child.tmp_free_mib",
         statvfs(tmp_dir.c_str(), &sv) == 0 ? std::to_string(sv.f_bavail * sv.f_frsize / (1024 * 1024)) : err("statvfs"));
    emit("child.threads", threads(64));
    if (network) emit("child.tcp_archive_org_443", tcp_connect("archive.org", "443"));
    emit("child.done", "1");
    std::fflush(stdout);
    return 0;
}

}  // namespace csprobe
