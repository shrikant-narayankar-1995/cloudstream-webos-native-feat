// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/top_bar.hpp"

#include "ui/theme/theme.hpp"

namespace csweb::ui {

using namespace wnk::ui;
using Tab = app::Router::Tab;

TopBar::TopBar(app::Router &router, Tab current) : Row(0) {
    set_padding({theme::kSafeX, theme::kSafeTop, theme::kSafeX, 0});
    set_align(Align::Stretch, Align::Start);

    auto &left = add<Row>(0);
    left.set_width(Dim::fill());
    auto &avatar = left.add<Label>("CS", TextRole::Body);
    avatar.set_bold(true).set_text_align(Align::Center);
    avatar.set_size(72, 72).set_background(theme::Color::hex(0x5B6CFFFF), 36);
    avatar.set_padding({0, 18, 0, 0});

    auto &nav = add<NavBar>();
    struct Item {
        const char *text, *icon;
        Tab tab;
    };
    const Item items[] = {{"Home", "", Tab::Home},
                          {"Search", "search", Tab::Search},
                          {"Library", "bookmark", Tab::Library},
                          {"Settings", "settings", Tab::Settings}};
    for (const auto &it : items) {
        const Tab tab = it.tab;
        auto &b = nav.add_tab(it.text, it.icon, [&router, tab](Ui &ui) { router.open_tab(ui, tab); });
        b.name = std::string("nav.") + it.text;
    }
    nav.set_selected(static_cast<size_t>(current));

    right_ = &add<Row>(20);
    right_->set_width(Dim::fill());
    right_->set_justify(Align::End);
    right_->set_align(Align::Stretch, Align::Center);
}

}  // namespace csweb::ui
