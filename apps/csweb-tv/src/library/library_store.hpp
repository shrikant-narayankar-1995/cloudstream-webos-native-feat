// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keeps the Library in the platform store (DB8 on the TV) as one chunked blob, "library". Read once
// at start (blocking); every change is written on a worker, newest snapshot wins (as SettingsStore).
// Screens listen to `changed` to refresh rows and buttons.
#pragma once

#include "core/signal.hpp"
#include "library/library.hpp"

#include "wnk/async/async.hpp"
#include "wnk/storage/kv.hpp"

#include <functional>
#include <memory>
#include <mutex>

namespace csweb::library {

class LibraryStore {
 public:
    LibraryStore(wnk::storage::KeyValueStore *kv, wnk::async::WorkerPool &pool);
    void load();

    const Library &get() const { return lib_; }
    // Applies `change` and saves.
    void update(const std::function<void(Library &)> &change);
    core::Signal changed;

 private:
    void save();
    wnk::storage::KeyValueStore *kv_;
    wnk::async::WorkerPool &pool_;
    Library lib_;
    struct Writer {
        std::mutex mu;
        uint64_t written = 0;
    };
    std::shared_ptr<Writer> writer_ = std::make_shared<Writer>();
    uint64_t seq_ = 0;
};

int64_t now_ms();  // wall clock, for updated times

}  // namespace csweb::library
