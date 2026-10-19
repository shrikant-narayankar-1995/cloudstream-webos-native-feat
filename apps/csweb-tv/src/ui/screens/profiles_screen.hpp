// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Who's watching?" (design 13), as CloudStream's AccountSelectActivity: a round avatar per profile
// and an "Add" tile; choosing a locked profile opens the PIN pad under the row, any other switches to
// it at once. "Manage profiles" turns the avatars into editors (rename, colour, set or remove the PIN,
// delete; the default profile cannot be deleted). At start (more than one profile, or a locked one)
// Back leaves the app instead of skipping the choice.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

#include <functional>

namespace csweb::ui {

class PillButton;
class PinPad;

class ProfilesScreen : public wnk::ui::Screen {
 public:
    // `on_chosen` runs after a profile has been chosen at start (e.g. a link waiting for it).
    ProfilesScreen(app::Services &s, bool startup, std::function<void(wnk::ui::Ui &)> on_chosen = {});
    void on_show(wnk::ui::Ui &ui) override;
    bool on_back(wnk::ui::Ui &ui) override;

 private:
    void rebuild();
    void choose(wnk::ui::Ui &ui, int key_index);
    void open_pin(wnk::ui::Ui &ui, int key_index);
    void close_pin(wnk::ui::Ui &ui);
    void add_profile(wnk::ui::Ui &ui);
    void edit(wnk::ui::Ui &ui, int key_index);
    void ask_pin(wnk::ui::Ui &ui, const std::string &title, std::function<void(wnk::ui::Ui &, const std::string &)> done);
    void save();

    app::Services &s_;
    bool startup_;
    bool managing_ = false;
    std::function<void(wnk::ui::Ui &)> on_chosen_;
    wnk::ui::Row *tiles_ = nullptr;
    PinPad *pin_ = nullptr;
    int pin_for_ = -1;
    wnk::ui::Label *hint_ = nullptr;
    PillButton *manage_ = nullptr;
    wnk::ui::View *first_ = nullptr;
};

// A dialog asking for a profile name (`done` gets what was typed).
void ask_profile_name(wnk::ui::Ui &ui, const std::string &title, const std::string &current,
                      std::function<void(wnk::ui::Ui &, const std::string &)> done);
// A dialog of the avatar colours; saves the choice and runs `changed`.
void choose_profile_colour(wnk::ui::Ui &ui, app::Services &s, int key_index, std::function<void(wnk::ui::Ui &)> changed);

// Asks for `key_index`'s PIN when it has one, then runs `then` (CloudStream's handleAccountSelect).
void unlock_profile(wnk::ui::Ui &ui, app::Services &s, int key_index, std::function<void(wnk::ui::Ui &)> then);

}  // namespace csweb::ui
