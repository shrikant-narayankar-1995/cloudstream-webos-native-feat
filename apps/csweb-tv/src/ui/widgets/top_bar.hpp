// SPDX-License-Identifier: GPL-3.0-or-later
//
// The top of every main screen (designs 01, 02, 09): the profile avatar on the left, the glass
// navigation pill in the middle, and a slot on the right (the Home screen puts the provider picker
// and the clock there).
#pragma once

#include "app/router.hpp"
#include "ui/widgets/nav_bar.hpp"

#include "wnk/ui/widgets.hpp"

namespace csweb::ui {

class TopBar : public wnk::ui::Row {
 public:
    TopBar(app::Router &router, app::Router::Tab current);
    wnk::ui::Row &right() { return *right_; }

 private:
    wnk::ui::Row *right_;
};

}  // namespace csweb::ui
