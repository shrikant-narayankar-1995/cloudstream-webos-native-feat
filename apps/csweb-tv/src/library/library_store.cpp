// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/library_store.hpp"

#include "wnk/log.hpp"
#include "wnk/storage/blob.hpp"

#include <chrono>

namespace csweb::library {

namespace {
constexpr const char *kBlob = "library";
constexpr size_t kMaxBytes = 4u << 20;
}  // namespace

std::string library_blob(int profile) { return profile == 0 ? kBlob : std::string(kBlob) + "-p" + std::to_string(profile); }

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

LibraryStore::LibraryStore(wnk::storage::KeyValueStore *kv, wnk::async::WorkerPool &pool) : kv_(kv), pool_(pool) {}

void LibraryStore::load() {
    if (!kv_) return;
    wnk::storage::BlobStore blobs(*kv_, kMaxBytes);
    std::string data;
    if (blobs.get(library_blob(profile_), &data) != wnk::storage::Status::Ok) return;
    lib_ = Library::from_json(nlohmann::json::parse(data, nullptr, false));
}

void LibraryStore::switch_profile(int profile) {
    if (profile == profile_) return;
    // Writes already queued carry their own key and still land.
    profile_ = profile;
    lib_ = Library();
    load();
    changed.emit();
}

void LibraryStore::erase_profile(int profile) {
    if (!kv_ || profile == 0 || profile == profile_) return;
    auto *kv = kv_;
    const std::string key = library_blob(profile);
    auto w = writer_;
    pool_.submit([kv, key, w](const wnk::async::CancellationToken &) {
        std::lock_guard<std::mutex> lock(w->mu);
        wnk::storage::BlobStore blobs(*kv, kMaxBytes);
        blobs.remove(key);
    });
}

void LibraryStore::update(const std::function<void(Library &)> &change) {
    change(lib_);
    save();
    changed.emit();
}

void LibraryStore::save() {
    if (!kv_) return;
    auto *kv = kv_;
    const std::string data = lib_.to_json().dump();
    const std::string key = library_blob(profile_);
    const uint64_t seq = ++seq_;
    auto w = writer_;
    pool_.submit([kv, data, key, seq, w](const wnk::async::CancellationToken &) {
        std::lock_guard<std::mutex> lock(w->mu);
        if (seq <= w->written[key]) return;
        w->written[key] = seq;
        wnk::storage::BlobStore blobs(*kv, kMaxBytes);
        if (blobs.put(key, data) != wnk::storage::Status::Ok) WNK_LOGW("csweb", "library: not saved (%zu bytes)", data.size());
    });
}

}  // namespace csweb::library
