// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings (design 09): the CloudStream settings this app has so far — Extensions, the source shown
// on Home — and what is running (app version, plugin runtime state).
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

namespace csweb::ui {

class SettingsScreen : public wnk::ui::Screen {
 public:
    explicit SettingsScreen(app::Services &s);
    void on_show(wnk::ui::Ui &ui) override;

 private:
    void refresh();
    app::Services &s_;
    wnk::ui::Label *runtime_ = nullptr, *home_ = nullptr;
    wnk::ui::View *first_ = nullptr;
    bool shown_ = false;
    core::Signal::Subscription sub_;
};


}  // namespace csweb::ui
