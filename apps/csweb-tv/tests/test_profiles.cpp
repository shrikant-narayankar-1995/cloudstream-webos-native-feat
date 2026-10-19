// SPDX-License-Identifier: GPL-3.0-or-later
// profiles/profiles and the per-profile settings keys.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "library/library_store.hpp"
#include "profiles/profiles.hpp"
#include "wnk/storage/blob.hpp"
#include "settings/settings_store.hpp"

using namespace csweb::profiles;

TEST("default_profile_always_exists") {
    Profiles p = Profiles::from_json(nlohmann::json());
    CHECK_EQ(p.all().size(), size_t(1));
    CHECK_EQ(p.current().key_index, 0);
    CHECK_EQ(p.current().name, std::string("Default"));
    CHECK(!p.remove(0));
    // Garbage and duplicates are dropped; a stored list without key 0 gets it back, first.
    p = Profiles::from_json(nlohmann::json::parse(
        R"({"profiles":[{"keyIndex":3,"name":"  Kids  ","imageIndex":99},{"keyIndex":3,"name":"dup"},{"name":"x"},5],"selected":3})"));
    CHECK_EQ(p.all().size(), size_t(2));
    CHECK_EQ(p.all()[0].key_index, 0);
    CHECK_EQ(p.all()[1].name, std::string("Kids"));
    CHECK_EQ(p.all()[1].image_index, 0);
    CHECK_EQ(p.selected(), 3);
    CHECK_EQ(Profiles::from_json(nlohmann::json::parse(R"({"selected":7})")).selected(), 0);
}

TEST("add_rename_remove_round_trip") {
    Profiles p = Profiles::from_json(nlohmann::json());
    const int a = p.add("Living room").key_index;
    const int b = p.add("").key_index;
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 2);
    CHECK_EQ(p.find(b)->name, std::string("Profile 2"));
    CHECK(p.rename(a, "Lounge"));
    CHECK(!p.rename(a, "   "));
    CHECK(p.select(b));
    CHECK(p.remove(b));
    CHECK_EQ(p.selected(), 0);  // the removed profile was selected
    CHECK_EQ(p.add("Next").key_index, 2);
    const Profiles q = Profiles::from_json(p.to_json());
    CHECK_EQ(q.all().size(), size_t(3));
    CHECK_EQ(q.find(1)->name, std::string("Lounge"));
    CHECK(!p.select(42));
}

TEST("avatar_colour_is_kept_and_bounded") {
    Profiles p = Profiles::from_json(nlohmann::json());
    const int k = p.add("Kids").key_index;
    CHECK(p.set_image(k, 4));
    CHECK(!p.set_image(k, kAvatarColours));
    CHECK(!p.set_image(k, -1));
    CHECK(!p.set_image(99, 1));
    const Profiles back = Profiles::from_json(p.to_json());  // CHECK_EQ keeps a reference: not into a temporary
    REQUIRE(back.find(k) != nullptr);
    CHECK_EQ(back.find(k)->image_index, 4);
}

TEST("pins_are_four_digits_and_stored_as_digests") {
    Profiles p = Profiles::from_json(nlohmann::json());
    const int k = p.add("Kids").key_index;
    CHECK(!p.set_pin(k, "12a4"));
    CHECK(!p.set_pin(k, "123"));
    CHECK(p.set_pin(k, "0420"));
    CHECK(p.find(k)->locked());
    CHECK(p.check_pin(k, "0420"));
    CHECK(!p.check_pin(k, "0421"));
    CHECK(p.check_pin(0, ""));  // not locked
    const std::string dumped = p.to_json().dump();
    CHECK(dumped.find("0420") == std::string::npos);
    CHECK(Profiles::from_json(p.to_json()).check_pin(k, "0420"));
    CHECK(p.set_pin(k, ""));
    CHECK(!p.find(k)->locked());
}

TEST("scoped_keys_and_initials") {
    CHECK_EQ(scoped_key(0, "library"), std::string("library"));
    CHECK_EQ(scoped_key(2, "library"), std::string("2/library"));
    // Store keys allow no "/": the library blob has its own naming.
    CHECK_EQ(csweb::library::library_blob(0), std::string("library"));
    CHECK_EQ(csweb::library::library_blob(3), std::string("library-p3"));
    CHECK(wnk::storage::BlobStore::is_valid_name(csweb::library::library_blob(12)));
    CHECK_EQ(initials("Living room"), std::string("LR"));
    CHECK_EQ(initials("kids"), std::string("K"));
    CHECK_EQ(initials("  "), std::string("?"));
}

TEST("settings_keep_profile_values_apart") {
    wnk::async::WorkerPool pool(1);
    csweb::settings::SettingsStore s(nullptr, pool);
    s.set(csweb::settings::kHomeProvider, "A");
    s.set("quality_pref_key", 720);
    s.set_profile(1);
    CHECK_EQ(s.get_string(csweb::settings::kHomeProvider), std::string(""));
    CHECK_EQ(s.get_int("quality_pref_key", 0), 720);  // shared
    s.set(csweb::settings::kHomeProvider, "B");
    s.set_profile(0);
    CHECK_EQ(s.get_string(csweb::settings::kHomeProvider), std::string("A"));
    s.erase_profile(1);
    s.set_profile(1);
    CHECK_EQ(s.get_string(csweb::settings::kHomeProvider), std::string(""));
}
