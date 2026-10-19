// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/stall_sampler.hpp"

#include "wnk/log.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <fstream>
#include <pthread.h>
#include <ucontext.h>
#include <unistd.h>

namespace csweb::core {

namespace {
int g_fd = -1;
pthread_t g_main;

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void write_str(const char *s) {
    if (g_fd >= 0) (void)!::write(g_fd, s, std::strlen(s));
}

uintptr_t g_text_lo = 0, g_text_hi = 0;  // the executable's code, for the stack scan

[[maybe_unused]] void write_hex(const char *label, uintptr_t v) {
    char buf[32];
    int i = sizeof buf;
    buf[--i] = '\n';
    do {
        buf[--i] = "0123456789abcdef"[v & 15];
        v >>= 4;
    } while (v != 0 && i > 2);
    buf[--i] = 'x';
    buf[--i] = '0';
    write_str(label);
    if (g_fd >= 0) (void)!::write(g_fd, buf + i, sizeof buf - static_cast<size_t>(i));
}

void on_signal(int, siginfo_t *, void *context) {
    write_str("-- main thread\n");
#if defined(__arm__)
    // ARM unwind tables stop at the signal frame: report the interrupted pc and lr, then every word on
    // the stack that points into the executable's code (return addresses, plus some noise).
    const auto *uc = static_cast<const ucontext_t *>(context);
    write_hex("pc ", uc->uc_mcontext.arm_pc);
    write_hex("lr ", uc->uc_mcontext.arm_lr);
    const auto *sp = reinterpret_cast<const uintptr_t *>(uc->uc_mcontext.arm_sp);
    int found = 0;
    for (int i = 0; i < 4096 && found < 40; i++) {
        const uintptr_t v = sp[i];
        if (v >= g_text_lo && v < g_text_hi) {
            write_hex("st ", v);
            ++found;
        }
    }
#else
    (void)context;
    void *frames[48];
    const int n = backtrace(frames, 48);
    if (g_fd >= 0) backtrace_symbols_fd(frames, n, g_fd);
#endif
}
}  // namespace

StallSampler::StallSampler(wnk::async::MainDispatcher &dispatcher, const std::string &path, int threshold_ms, int max_samples)
    : dispatcher_(dispatcher), threshold_ms_(threshold_ms), max_samples_(max_samples),
      answered_(std::make_shared<std::atomic<int64_t>>(now_ms())) {
    g_fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (g_fd < 0) {
        WNK_LOGW("csweb", "stall sampler: cannot open %s", path.c_str());
        return;
    }
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
        if (line.find(" r-xp ") == std::string::npos) continue;
        write_str((line + "\n").c_str());
        char exe[4096];
        const ssize_t n = ::readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0 && g_text_hi == 0 && line.find(std::string(exe, static_cast<size_t>(n))) != std::string::npos) {
            g_text_lo = std::stoul(line.substr(0, line.find('-')), nullptr, 16);
            g_text_hi = std::stoul(line.substr(line.find('-') + 1), nullptr, 16);
        }
    }
    void *warm[1];
    backtrace(warm, 1);  // loads libgcc_s outside the handler
    g_main = pthread_self();
    struct sigaction sa {};
    sa.sa_sigaction = on_signal;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, nullptr);
    thread_ = std::thread(&StallSampler::watch, this);
}

StallSampler::~StallSampler() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (g_fd >= 0) ::close(g_fd);
    g_fd = -1;
}

void StallSampler::watch() {
    int samples = 0;
    int64_t posted = 0, last_sample = 0;
    while (!stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int64_t t = now_ms();
        if (answered_->load() >= posted && t - posted >= 250) {
            posted = t;
            auto answered = answered_;
            dispatcher_.post([answered, t] { answered->store(t); });
        }
        const int64_t waited = t - posted;
        if (answered_->load() < posted && waited > threshold_ms_ && t - last_sample >= 500 && samples < max_samples_) {
            last_sample = t;
            ++samples;
            char head[96];
            std::snprintf(head, sizeof head, "== stall %lld ms (sample %d)\n", static_cast<long long>(waited), samples);
            write_str(head);
            pthread_kill(g_main, SIGUSR2);
        }
    }
}

}  // namespace csweb::core
