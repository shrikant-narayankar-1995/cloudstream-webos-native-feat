// SPDX-License-Identifier: GPL-3.0-or-later
//
// The floating glass navigation pill at the top of the main screens (PlxNative's layout), holding
// CloudStream's main destinations except Downloads (a Developer Mode TV has no storage for video):
// Home, Search, Library, Settings. A focus scope that
// remembers its tab, so Up from the content returns to the tab that was used.
#pragma once

#include "ui/widgets/pill_button.hpp"

#include <functional>
#include <string>
#include <vector>

namespace csweb::ui {

class NavBar : public wnk::ui::Row {
 public:
    NavBar();
    PillButton &add_tab(std::string text, std::string icon, wnk::ui::ClickHandler on_click);
    void set_selected(size_t index);

 protected:
    void draw(wnk::ui::Ui &ui) override;

 private:
    std::vector<PillButton *> tabs_;
};

}  // namespace csweb::ui
