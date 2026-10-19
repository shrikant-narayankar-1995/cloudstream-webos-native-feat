// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings/settings_store.hpp"

#include "profiles/profiles.hpp"

namespace csweb::settings {

namespace {
constexpr const char *kKey = "csweb.settings.v1";
}

bool profile_key(const std::string &key) {
    // home_api_used, search history, and the type chips (UserPreferenceDelegate) are per account.
    return key == kHomeProvider || key == kSearchHistory || key == "search_pref_tags" || key == "home_pref_homepage";
}

std::string SettingsStore::key_for(const std::string &key) const {
    return profile_key(key) ? profiles::scoped_key(profile_, key) : key;
}

SettingsStore::SettingsStore(wnk::storage::KeyValueStore *kv, wnk::async::WorkerPool &pool) : kv_(kv), pool_(pool) {}

void SettingsStore::load() {
    if (!kv_) return;
    wnk::storage::Entry e;
    if (kv_->get(kKey, &e) != wnk::storage::Status::Ok) return;
    auto j = nlohmann::json::parse(e.value, nullptr, false);
    if (j.is_object()) values_ = std::move(j);
}

std::string SettingsStore::get_string(const std::string &key, const std::string &def) const {
    auto it = values_.find(key_for(key));
    return it != values_.end() && it->is_string() ? it->get<std::string>() : def;
}

bool SettingsStore::get_bool(const std::string &key, bool def) const {
    auto it = values_.find(key_for(key));
    return it != values_.end() && it->is_boolean() ? it->get<bool>() : def;
}

long long SettingsStore::get_int(const std::string &key, long long def) const {
    auto it = values_.find(key_for(key));
    return it != values_.end() && it->is_number_integer() ? it->get<long long>() : def;
}

nlohmann::json SettingsStore::get(const std::string &key) const {
    auto it = values_.find(key_for(key));
    return it != values_.end() ? *it : nlohmann::json();
}

void SettingsStore::set(const std::string &key, nlohmann::json value) {
    const std::string k = key_for(key);
    if (values_.contains(k) && values_[k] == value) return;
    values_[k] = std::move(value);
    save();
}

void SettingsStore::erase_profile(int profile) {
    if (profile == 0) return;
    const std::string prefix = std::to_string(profile) + "/";
    bool changed = false;
    for (auto it = values_.begin(); it != values_.end();) {
        if (it.key().compare(0, prefix.size(), prefix) == 0) {
            it = values_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) save();
}

void SettingsStore::save() {
    if (!kv_) return;
    auto *kv = kv_;
    const std::string data = values_.dump();
    const uint64_t seq = ++seq_;
    auto w = writer_;
    pool_.submit([kv, data, seq, w](const wnk::async::CancellationToken &) {
        std::lock_guard<std::mutex> lock(w->mu);
        if (seq <= w->written) return;
        w->written = seq;
        kv->put(kKey, data);
    });
}

}  // namespace csweb::settings
