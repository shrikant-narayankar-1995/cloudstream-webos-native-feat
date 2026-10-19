// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local profiles, as CloudStream's accounts (DataStoreHelper.Account, AccountSelectActivity): profile 0
// always exists ("Default"), others get the next key index; one is selected. Each profile has its own
// library (bookmarks, resume, positions), home source, type chips and search history; settings are
// shared. A profile may be locked with a 4-digit PIN. CloudStream stores the PIN as typed; this app
// stores a SHA-256 digest of it, so the settings blob does not hold the PIN itself.
#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace csweb::profiles {

struct Profile {
    int key_index = 0;
    std::string name;
    int image_index = 0;   // the avatar colour
    std::string pin_hash;  // empty: not locked
    bool locked() const { return !pin_hash.empty(); }
};

inline constexpr int kAvatarColours = 6;

class Profiles {
 public:
    static Profiles from_json(const nlohmann::json &j);
    nlohmann::json to_json() const;

    // Every profile, the default (key 0) first.
    const std::vector<Profile> &all() const { return list_; }
    const Profile *find(int key_index) const;
    const Profile &current() const;
    int selected() const { return selected_; }
    bool select(int key_index);  // false if there is no such profile

    Profile &add(const std::string &name);
    bool remove(int key_index);  // the default profile cannot be removed
    bool rename(int key_index, const std::string &name);
    bool set_image(int key_index, int image_index);  // false for an unknown profile or colour
    bool set_pin(int key_index, const std::string &pin);  // "" removes it; false for an invalid PIN
    bool check_pin(int key_index, const std::string &pin) const;

 private:
    Profiles();
    void ensure_default();
    std::vector<Profile> list_;
    int selected_ = 0;
};

bool valid_pin(const std::string &pin);  // four digits
std::string pin_digest(const std::string &pin);
// The name of a per-profile value: the key itself for the default profile (data from before profiles
// keeps working), "<index>/<key>" otherwise (CloudStream's "$currentAccount/key").
std::string scoped_key(int key_index, const std::string &key);
// Up to two initials for the avatar ("Living Room" -> "LR").
std::string initials(const std::string &name);

}  // namespace csweb::profiles
