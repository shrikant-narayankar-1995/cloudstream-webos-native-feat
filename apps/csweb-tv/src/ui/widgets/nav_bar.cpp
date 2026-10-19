// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/nav_bar.hpp"

#include "ui/theme/theme.hpp"

namespace csweb::ui {

using namespace wnk::ui;

NavBar::NavBar() : Row(4) {
    set_padding(Insets::all(6));
    set_focus_scope(false, true);
    set_align(Align::Center, Align::Start);
}

PillButton &NavBar::add_tab(std::string text, std::string icon, ClickHandler on_click) {
    auto &b = add<PillButton>(std::move(text), std::move(icon), PillButton::Style::Ghost, std::move(on_click));
    tabs_.push_back(&b);
    return b;
}

void NavBar::set_selected(size_t index) {
    for (size_t i = 0; i < tabs_.size(); i++) tabs_[i]->set_selected(i == index);
}

void NavBar::draw(Ui &ui) {
    const wnk::Rect f = frame();
    auto &r = ui.renderer();
    r.fill_rect(f, ui.fade(theme::kGlassBorder), f.h * 0.5f);
    r.fill_rect({f.x + 1, f.y + 1, f.w - 2, f.h - 2}, ui.fade(theme::Color::hex(0x161921E6)), f.h * 0.5f - 1);
}

}  // namespace csweb::ui
