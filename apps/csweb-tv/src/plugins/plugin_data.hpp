// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plugin data (what plugins save through SharedPreferences and getFilesDir) lives in the runtime's
// data directory on tmpfs, which a reboot empties; on Android it would persist. This keeps a copy in
// the store (DB8 blob "plugin-data"): restored before the runtime starts, saved when a plugin's
// settings dialog ends, when the app goes to the background and when it stops. Unchanged data is
// not written again. The cache directory is not kept, as Android may clear it too.
#pragma once

#include "wnk/async/async.hpp"
#include "wnk/storage/kv.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace csweb::plugins {

class PluginDataStore {
 public:
    PluginDataStore(wnk::storage::KeyValueStore *kv, std::string data_dir);
    void restore();                          // blocking, before the runtime starts
    void save(wnk::async::WorkerPool &pool);  // on a worker
    void save_now();                         // blocking (on_stop)

 private:
    struct Shared {
        std::mutex mu;
        size_t last_hash = 0;
    };
    static void save_impl(wnk::storage::KeyValueStore *kv, const std::string &dir, Shared &shared);
    wnk::storage::KeyValueStore *kv_;
    std::string dir_;
    std::shared_ptr<Shared> shared_ = std::make_shared<Shared>();
};

}  // namespace csweb::plugins
