// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugins/plugin_data.hpp"

#include "core/dir_snapshot.hpp"

#include "wnk/log.hpp"
#include "wnk/storage/blob.hpp"

#include <functional>

namespace csweb::plugins {

namespace {
constexpr const char *kBlob = "plugin-data";
constexpr size_t kMaxBlob = 4u << 20;  // base64 of the 2 MiB snapshot limit, with room
const std::vector<std::string> kSubdirs{"shared_prefs", "files"};
}  // namespace

PluginDataStore::PluginDataStore(wnk::storage::KeyValueStore *kv, std::string data_dir) : kv_(kv), dir_(std::move(data_dir)) {}

void PluginDataStore::restore() {
    if (!kv_) return;
    wnk::storage::BlobStore blobs(*kv_, kMaxBlob);
    std::string data;
    if (blobs.get(kBlob, &data) != wnk::storage::Status::Ok) return;
    size_t rejected = 0;
    const size_t n = core::restore_dir(dir_, nlohmann::json::parse(data, nullptr, false), kSubdirs, &rejected);
    std::lock_guard<std::mutex> lock(shared_->mu);
    shared_->last_hash = std::hash<std::string>{}(data);
    WNK_LOGI("csweb", "plugin data: %zu files restored%s", n, rejected ? " (some rejected)" : "");
}

void PluginDataStore::save_impl(wnk::storage::KeyValueStore *kv, const std::string &dir, Shared &shared) {
    size_t skipped = 0;
    const std::string data = core::snapshot_dir(dir, kSubdirs, {}, &skipped).dump();
    const size_t h = std::hash<std::string>{}(data);
    std::lock_guard<std::mutex> lock(shared.mu);
    if (h == shared.last_hash) return;
    wnk::storage::BlobStore blobs(*kv, kMaxBlob);
    if (blobs.put(kBlob, data) != wnk::storage::Status::Ok) {
        WNK_LOGW("csweb", "plugin data: not saved (%zu bytes)", data.size());
        return;
    }
    shared.last_hash = h;
    WNK_LOGI("csweb", "plugin data: saved (%zu bytes%s)", data.size(), skipped ? ", some files over the limits" : "");
}

void PluginDataStore::save(wnk::async::WorkerPool &pool) {
    if (!kv_) return;
    auto *kv = kv_;
    auto shared = shared_;
    const std::string dir = dir_;
    pool.submit([kv, shared, dir](const wnk::async::CancellationToken &) { save_impl(kv, dir, *shared); });
}

void PluginDataStore::save_now() {
    if (kv_) save_impl(kv_, dir_, *shared_);
}

}  // namespace csweb::plugins
