// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings/settings_store.hpp"

namespace csweb::settings {

namespace {
constexpr const char *kKey = "csweb.settings.v1";
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
    auto it = values_.find(key);
    return it != values_.end() && it->is_string() ? it->get<std::string>() : def;
}

bool SettingsStore::get_bool(const std::string &key, bool def) const {
    auto it = values_.find(key);
    return it != values_.end() && it->is_boolean() ? it->get<bool>() : def;
}

long long SettingsStore::get_int(const std::string &key, long long def) const {
    auto it = values_.find(key);
    return it != values_.end() && it->is_number_integer() ? it->get<long long>() : def;
}

void SettingsStore::set(const std::string &key, nlohmann::json value) {
    if (values_.contains(key) && values_[key] == value) return;
    values_[key] = std::move(value);
    save();
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
