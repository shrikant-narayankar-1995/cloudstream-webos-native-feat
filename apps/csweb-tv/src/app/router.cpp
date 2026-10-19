// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/router.hpp"

#include "ui/screens/details_screen.hpp"
#include "ui/screens/extensions_screen.hpp"
#include "ui/screens/home_screen.hpp"
#include "ui/screens/player_screen.hpp"
#include "ui/screens/search_screen.hpp"
#include "ui/screens/settings_screen.hpp"

namespace csweb::app {

Router::Router(Services &services, wnk::ui::Navigator &nav) : s_(services), nav_(nav) {}

void Router::open_tab(wnk::ui::Ui &ui, Tab tab) {
    nav_.pop_to_root(ui);  // Home is the root
    switch (tab) {
        case Tab::Home: break;
        case Tab::Search: nav_.push(ui, std::make_unique<ui::SearchScreen>(s_)); break;
        case Tab::Library:
            nav_.push(ui, std::make_unique<ui::PendingTabScreen>(s_, tab, "Library",
                                                                 "Bookmarks and watch history are not built yet."));
            break;
        case Tab::Downloads:
            nav_.push(ui, std::make_unique<ui::PendingTabScreen>(s_, tab, "Downloads", "Downloads are not built yet."));
            break;
        case Tab::Settings: nav_.push(ui, std::make_unique<ui::SettingsScreen>(s_)); break;
    }
}

void Router::open_details(wnk::ui::Ui &ui, const std::string &provider, const runtime::SearchResult &item) {
    nav_.push(ui, std::make_unique<ui::DetailsScreen>(s_, provider.empty() ? item.api_name : provider, item));
}

void Router::open_sources(wnk::ui::Ui &ui, const PlayTarget &target) {
    nav_.push(ui, std::make_unique<ui::PlayerScreen>(s_, target));
}

void Router::open_extensions(wnk::ui::Ui &ui) { nav_.push(ui, std::make_unique<ui::ExtensionsScreen>(s_)); }

void Router::open_repository(wnk::ui::Ui &ui, const plugins::RepositoryData &repo) {
    nav_.push(ui, std::make_unique<ui::RepositoryScreen>(s_, repo));
}

void Router::back(wnk::ui::Ui &ui) { nav_.pop(ui); }

}  // namespace csweb::app
