// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keeps the Library in the platform store (DB8 on the TV) as one chunked blob per profile: "library"
// for the default profile, "library-p<profile>" for the others (store keys are [a-z0-9._-]). Read at start and on a profile switch
// (blocking); every change is written on a worker, newest snapshot wins (as SettingsStore).
// Screens listen to `changed` to refresh rows and buttons.
#pragma once

#include "core/signal.hpp"
#include "library/library.hpp"

#include "wnk/async/async.hpp"
#include "wnk/storage/kv.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace csweb::library {

class LibraryStore {
 public:
    LibraryStore(wnk::storage::KeyValueStore *kv, wnk::async::WorkerPool &pool);
    void load();
    // Saves nothing; reads `profile`'s library and emits changed.
    void switch_profile(int profile);
    // Deletes a removed profile's library.
    void erase_profile(int profile);

    const Library &get() const { return lib_; }
    // Applies `change` and saves.
    void update(const std::function<void(Library &)> &change);
    core::Signal changed;

 private:
    void save();
    wnk::storage::KeyValueStore *kv_;
    wnk::async::WorkerPool &pool_;
    Library lib_;
    int profile_ = 0;
    struct Writer {
        std::mutex mu;
        std::map<std::string, uint64_t> written;  // per blob: a switch must not drop the other profile's write
    };
    std::shared_ptr<Writer> writer_ = std::make_shared<Writer>();
    uint64_t seq_ = 0;
};

// The blob holding `profile`'s library.
std::string library_blob(int profile);

int64_t now_ms();  // wall clock, for updated times

}  // namespace csweb::library
