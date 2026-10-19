// SPDX-License-Identifier: GPL-3.0-or-later
//
// User settings, one JSON object in the key/value store (DB8 on the TV). Read once at start; each
// change is written back on a worker. Keys follow CloudStream's setting names where one exists
// (e.g. "home_api_used" for the provider shown on Home, DataStoreHelper.currentHomePage).
// A few values belong to the selected profile (profile_key below): they are stored under
// "<profile>/<key>" for profiles other than the default, as CloudStream's per-account keys.
#pragma once

#include "wnk/async/async.hpp"
#include "wnk/storage/kv.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace csweb::settings {

class SettingsStore {
 public:
    SettingsStore(wnk::storage::KeyValueStore *kv, wnk::async::WorkerPool &pool);
    // Blocking read (one key): call from on_start.
    void load();

    std::string get_string(const std::string &key, const std::string &def = {}) const;
    bool get_bool(const std::string &key, bool def) const;
    long long get_int(const std::string &key, long long def) const;
    // The stored value, null when unset.
    nlohmann::json get(const std::string &key) const;
    void set(const std::string &key, nlohmann::json value);
    // Removes every per-profile value of `profile` (the profile was deleted).
    void erase_profile(int profile);
    void set_profile(int profile) { profile_ = profile; }
    int profile() const { return profile_; }

 private:
    void save();
    std::string key_for(const std::string &key) const;
    int profile_ = 0;
    wnk::storage::KeyValueStore *kv_;
    wnk::async::WorkerPool &pool_;
    nlohmann::json values_ = nlohmann::json::object();
    // Writes run on the pool and may overlap: each carries a sequence number and only the newest is
    // stored, under one lock, so an older snapshot never lands after a newer one.
    struct Writer {
        std::mutex mu;
        uint64_t written = 0;
    };
    std::shared_ptr<Writer> writer_ = std::make_shared<Writer>();
    uint64_t seq_ = 0;
};

// Setting keys
inline constexpr const char *kHomeProvider = "home_api_used";
inline constexpr const char *kNoneProvider = "NONE";      // APIRepository.noneApi
inline constexpr const char *kRandomProvider = "Random";  // APIRepository.randomApi
inline constexpr const char *kSearchHistory = "search_history";
inline constexpr const char *kProfiles = "profiles";  // profiles::Profiles (shared)
inline constexpr const char *kSkipProfileSelect = "skip_startup_account_select_key";
// True for the values each profile keeps for itself (CloudStream's $currentAccount keys).
bool profile_key(const std::string &key);

}  // namespace csweb::settings
