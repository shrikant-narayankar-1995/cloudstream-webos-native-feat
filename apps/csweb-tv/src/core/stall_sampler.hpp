// SPDX-License-Identifier: GPL-3.0-or-later
//
// Diagnostics for a main thread that stops answering (the UI hangs or crawls): a watcher thread posts
// a ping to the main dispatcher every 250 ms; when one has waited longer than `threshold_ms` it signals
// the main thread (SIGUSR2), whose handler appends a raw backtrace to `path`, every 500 ms while the
// stall lasts, at most `max_samples` in all. The file starts with the executable mappings of
// /proc/self/maps, so the addresses can be symbolised offline with the unstripped binary
// (addr2line -e <binary> <address - mapping start>). Nothing else is written: no URLs, no user data.
#pragma once

#include "wnk/async/async.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace csweb::core {

class StallSampler {
 public:
    // Call on the main thread.
    StallSampler(wnk::async::MainDispatcher &dispatcher, const std::string &path, int threshold_ms = 1500,
                 int max_samples = 60);
    ~StallSampler();

    StallSampler(const StallSampler &) = delete;
    StallSampler &operator=(const StallSampler &) = delete;

 private:
    void watch();

    wnk::async::MainDispatcher &dispatcher_;
    int threshold_ms_, max_samples_;
    std::shared_ptr<std::atomic<int64_t>> answered_;  // ms of the last ping the main thread ran
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

}  // namespace csweb::core
