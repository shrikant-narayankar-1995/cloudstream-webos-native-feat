// SPDX-License-Identifier: GPL-3.0-or-later
//
// Profiles (design 13), as CloudStream's AccountSelectActivity: a tile per profile with its avatar;
// choosing one asks for its PIN when it has one and switches to it. "Manage" turns the tiles into
// editors (rename, set or remove the PIN, delete; the default profile cannot be deleted) and offers
// "Add profile". At start (more than one profile, or a locked one) Back leaves the app instead of
// skipping the choice.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

#include <functional>

namespace csweb::ui {

class PillButton;

class ProfilesScreen : public wnk::ui::Screen {
 public:
    // `on_chosen` runs after a profile has been chosen at start (e.g. a link waiting for it).
    ProfilesScreen(app::Services &s, bool startup, std::function<void(wnk::ui::Ui &)> on_chosen = {});
    void on_show(wnk::ui::Ui &ui) override;
    bool on_back(wnk::ui::Ui &ui) override;

 private:
    void rebuild();
    void choose(wnk::ui::Ui &ui, int key_index);
    void edit(wnk::ui::Ui &ui, int key_index);
    void ask_name(wnk::ui::Ui &ui, const std::string &title, const std::string &current,
                  std::function<void(wnk::ui::Ui &, const std::string &)> done);
    void ask_pin(wnk::ui::Ui &ui, const std::string &title, std::function<void(wnk::ui::Ui &, const std::string &)> done);
    void save();

    app::Services &s_;
    bool startup_;
    bool managing_ = false;
    std::function<void(wnk::ui::Ui &)> on_chosen_;
    wnk::ui::Row *tiles_ = nullptr;
    wnk::ui::Label *hint_ = nullptr;
    PillButton *manage_ = nullptr;
    wnk::ui::View *first_ = nullptr;
};

// Asks for `key_index`'s PIN when it has one, then runs `then` (CloudStream's handleAccountSelect).
void unlock_profile(wnk::ui::Ui &ui, app::Services &s, int key_index, std::function<void(wnk::ui::Ui &)> then);

}  // namespace csweb::ui
