// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/settings_screen.hpp"

#include "ui/theme/theme.hpp"
#include "ui/widgets/pill_button.hpp"
#include "ui/widgets/top_bar.hpp"

#include "wnk/ui/forms.hpp"

#ifndef CSWEB_VERSION
#define CSWEB_VERSION "dev"
#endif

namespace csweb::ui {

using namespace wnk::ui;

namespace {
Column &tab_page(View &screen, app::Services &s, app::Router::Tab tab, const std::string &title) {
    auto &scroll = screen.add<ScrollView>();
    scroll.margin = 160;
    auto &col = scroll.add<Column>(22);
    col.set_padding({theme::kSafeX, 200, theme::kSafeX, 80});
    col.add<Label>(title, TextRole::Title).set_font_size(theme::kDisplay).set_bold(true);
    screen.add<TopBar>(*s.router, tab);
    return col;
}
}  // namespace

SettingsScreen::SettingsScreen(app::Services &s) : Screen("Settings"), s_(s) {
    name = "settings";
    auto &col = tab_page(*this, s_, app::Router::Tab::Settings, "Settings");
    auto &list = col.add<Column>(12);
    list.set_width(Dim::fixed(1200));
    first_ = &list.add<ValueRow>("Extensions", "Repositories and plugins", [this](Ui &ui) { s_.router->open_extensions(ui); });
    first_->name = "settings.extensions";
    home_ = &list.add<Label>("");
    home_->set_muted(true);
    runtime_ = &list.add<Label>("");
    runtime_->set_muted(true).set_max_lines(3);
    list.add<Label>(std::string("CSWeb TV ") + CSWEB_VERSION + " · runs CloudStream plugins on this TV").set_muted(true);
    sub_ = s_.plugins_changed.subscribe([this] { refresh(); });
}

void SettingsScreen::on_show(Ui &ui) {
    Screen::on_show(ui);
    if (!shown_) ui.focus(first_);
    shown_ = true;
    refresh();
}

void SettingsScreen::refresh() {
    const std::string home = s_.settings->get_string(settings::kHomeProvider);
    home_->set_text("Home source: " + (home.empty() ? std::string("first available") : home) + " (change it on Home)");
    std::string state;
    switch (s_.runtime->state()) {
        case runtime::RuntimeClient::State::Stopped: state = "stopped"; break;
        case runtime::RuntimeClient::State::Starting: state = "starting"; break;
        case runtime::RuntimeClient::State::Ready: state = "running"; break;
        case runtime::RuntimeClient::State::Failed: state = "failed: " + s_.runtime->last_error(); break;
    }
    runtime_->set_text("Plugin runtime: " + state + " · " + std::to_string(s_.plugins->providers().size()) +
                       " providers loaded");
}

PendingTabScreen::PendingTabScreen(app::Services &s, app::Router::Tab tab, std::string title, std::string text)
    : Screen(title) {
    name = "tab." + title;
    auto &col = tab_page(*this, s, tab, title);
    col.add<Label>(std::move(text)).set_muted(true);
}

}  // namespace csweb::ui
