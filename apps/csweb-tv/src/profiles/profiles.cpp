// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/profiles.hpp"

#include "core/sha256.hpp"

#include <algorithm>
#include <cctype>

namespace csweb::profiles {

namespace {
constexpr size_t kMaxName = 40;

std::string clean_name(std::string name) {
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    size_t start = 0;
    while (start < name.size() && std::isspace(static_cast<unsigned char>(name[start]))) start++;
    name.erase(0, start);
    if (name.size() > kMaxName) {
        name.resize(kMaxName);
        while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) name.pop_back();  // whole code points
        if (!name.empty() && (static_cast<unsigned char>(name.back()) & 0x80)) name.pop_back();
    }
    return name;
}
}  // namespace

Profiles::Profiles() { ensure_default(); }

void Profiles::ensure_default() {
    if (find(0) == nullptr) list_.insert(list_.begin(), Profile{0, "Default", 0, {}});
    std::stable_sort(list_.begin(), list_.end(), [](const Profile &a, const Profile &b) { return (a.key_index == 0) > (b.key_index == 0); });
}

Profiles Profiles::from_json(const nlohmann::json &j) {
    Profiles p;
    p.list_.clear();
    if (j.is_object() && j.contains("profiles") && j["profiles"].is_array()) {
        for (const auto &e : j["profiles"]) {
            if (!e.is_object() || !e.contains("keyIndex") || !e["keyIndex"].is_number_integer()) continue;
            Profile pr;
            pr.key_index = e["keyIndex"].get<int>();
            if (pr.key_index < 0 || p.find(pr.key_index) != nullptr) continue;
            pr.name = clean_name(e.value("name", ""));
            if (pr.name.empty()) pr.name = pr.key_index == 0 ? "Default" : "Profile " + std::to_string(pr.key_index);
            const int img = e.contains("imageIndex") && e["imageIndex"].is_number_integer() ? e["imageIndex"].get<int>() : 0;
            pr.image_index = img >= 0 && img < kAvatarColours ? img : 0;
            pr.pin_hash = e.value("pinHash", "");
            if (pr.pin_hash.size() != 64) pr.pin_hash.clear();
            p.list_.push_back(std::move(pr));
        }
    }
    p.ensure_default();
    const int sel = j.is_object() && j.contains("selected") && j["selected"].is_number_integer() ? j["selected"].get<int>() : 0;
    p.selected_ = p.find(sel) != nullptr ? sel : 0;
    return p;
}

nlohmann::json Profiles::to_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &p : list_) {
        nlohmann::json e = {{"keyIndex", p.key_index}, {"name", p.name}, {"imageIndex", p.image_index}};
        if (p.locked()) e["pinHash"] = p.pin_hash;
        arr.push_back(std::move(e));
    }
    return {{"profiles", arr}, {"selected", selected_}};
}

const Profile *Profiles::find(int key_index) const {
    for (const auto &p : list_) {
        if (p.key_index == key_index) return &p;
    }
    return nullptr;
}

const Profile &Profiles::current() const {
    const Profile *p = find(selected_);
    return p != nullptr ? *p : list_.front();
}

bool Profiles::select(int key_index) {
    if (find(key_index) == nullptr) return false;
    selected_ = key_index;
    return true;
}

Profile &Profiles::add(const std::string &name) {
    int next = 0;
    for (const auto &p : list_) next = std::max(next, p.key_index);
    Profile p;
    p.key_index = next + 1;
    p.name = clean_name(name);
    if (p.name.empty()) p.name = "Profile " + std::to_string(p.key_index);
    p.image_index = p.key_index % kAvatarColours;
    list_.push_back(std::move(p));
    return list_.back();
}

bool Profiles::remove(int key_index) {
    if (key_index == 0) return false;
    const auto it = std::find_if(list_.begin(), list_.end(), [&](const Profile &p) { return p.key_index == key_index; });
    if (it == list_.end()) return false;
    list_.erase(it);
    if (selected_ == key_index) selected_ = 0;
    return true;
}

bool Profiles::rename(int key_index, const std::string &name) {
    const std::string n = clean_name(name);
    if (n.empty()) return false;
    for (auto &p : list_) {
        if (p.key_index != key_index) continue;
        p.name = n;
        return true;
    }
    return false;
}

bool Profiles::set_pin(int key_index, const std::string &pin) {
    if (!pin.empty() && !valid_pin(pin)) return false;
    for (auto &p : list_) {
        if (p.key_index != key_index) continue;
        p.pin_hash = pin.empty() ? std::string() : pin_digest(pin);
        return true;
    }
    return false;
}

bool Profiles::check_pin(int key_index, const std::string &pin) const {
    const Profile *p = find(key_index);
    if (p == nullptr) return false;
    return !p->locked() || (valid_pin(pin) && pin_digest(pin) == p->pin_hash);
}

bool valid_pin(const std::string &pin) {
    return pin.size() == 4 && std::all_of(pin.begin(), pin.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}

std::string pin_digest(const std::string &pin) { return core::Sha256::hex("csweb-profile-pin:" + pin); }

std::string scoped_key(int key_index, const std::string &key) {
    return key_index == 0 ? key : std::to_string(key_index) + "/" + key;
}

std::string initials(const std::string &name) {
    std::string out;
    bool start = true;
    for (size_t i = 0; i < name.size() && out.size() < 2; i++) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (std::isspace(c)) {
            start = true;
            continue;
        }
        if (start && c < 0x80 && std::isalnum(c)) out += static_cast<char>(std::toupper(c));
        start = false;
    }
    return out.empty() ? "?" : out;
}

}  // namespace csweb::profiles
