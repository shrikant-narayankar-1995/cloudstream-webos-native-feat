// SPDX-License-Identifier: GPL-3.0-or-later
//
// First-run setup (design 14), shown once, as CloudStream's setup fragments are until
// HAS_DONE_SETUP_KEY is set: a welcome that says the app has no content of its own, "Who's
// watching?" where the people who watch are added as profiles (name, colour), and a last step that
// leads to Extensions or Home. Back goes to the previous step; on the first one it leaves the app.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

#include <functional>

namespace csweb::ui {

class PillButton;

class SetupScreen : public wnk::ui::Screen {
 public:
    // `done` runs once the setup is finished (and saved); `open_extensions` when the user asked
    // for Extensions rather than Home.
    SetupScreen(app::Services &s, std::function<void(wnk::ui::Ui &, bool open_extensions)> done);
    void on_show(wnk::ui::Ui &ui) override;
    bool on_back(wnk::ui::Ui &ui) override;

 private:
    void show_step(wnk::ui::Ui *ui, int step);
    void build_steps();
    void build_welcome();
    void build_profiles();
    void build_finish();
    void edit_profile(wnk::ui::Ui &ui, int key_index);
    void add_profile(wnk::ui::Ui &ui);
    void finish(wnk::ui::Ui &ui, bool open_extensions);

    app::Services &s_;
    std::function<void(wnk::ui::Ui &, bool)> done_;
    int step_ = 0;
    wnk::ui::Row *steps_ = nullptr;
    wnk::ui::Column *main_ = nullptr;
    wnk::ui::Column *side_ = nullptr;
    wnk::ui::View *first_ = nullptr;
    int focus_key_ = -1;  // a profile to focus after the row is rebuilt
};

}  // namespace csweb::ui
