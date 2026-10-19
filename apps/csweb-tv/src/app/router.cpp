// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/router.hpp"

#include "ui/screens/details_screen.hpp"
#include "ui/screens/extensions_screen.hpp"
#include "ui/screens/home_screen.hpp"
#include "ui/screens/library_screen.hpp"
#include "ui/screens/player_screen.hpp"
#include "ui/screens/profiles_screen.hpp"
#include "ui/screens/provider_test_screen.hpp"
#include "ui/screens/search_screen.hpp"
#include "ui/screens/settings_screen.hpp"

namespace csweb::app {

Router::Router(Services &services, wnk::ui::Navigator &nav) : s_(services), nav_(nav) {}

void Router::open_tab(wnk::ui::Ui &ui, Tab tab) {
    nav_.pop_to_root(ui);  // Home is the root
    switch (tab) {
        case Tab::Home: break;
        case Tab::Search: nav_.push(ui, std::make_unique<ui::SearchScreen>(s_)); break;
        case Tab::Library: nav_.push(ui, std::make_unique<ui::LibraryScreen>(s_)); break;
        case Tab::Settings: nav_.push(ui, std::make_unique<ui::SettingsScreen>(s_)); break;
    }
}

void Router::open_search(wnk::ui::Ui &ui, const std::string &query) {
    nav_.pop_to_root(ui);
    auto &screen = static_cast<ui::SearchScreen &>(nav_.push(ui, std::make_unique<ui::SearchScreen>(s_)));
    screen.start_query(ui, query);
}

void Router::open_details(wnk::ui::Ui &ui, const std::string &provider, const runtime::SearchResult &item, bool resume) {
    nav_.push(ui, std::make_unique<ui::DetailsScreen>(s_, provider.empty() ? item.api_name : provider, item, resume));
}

void Router::open_sources(wnk::ui::Ui &ui, const PlayTarget &target) {
    nav_.push(ui, std::make_unique<ui::PlayerScreen>(s_, target));
}

void Router::replace_player(wnk::ui::Ui &ui, const PlayTarget &target) {
    if (dynamic_cast<ui::PlayerScreen *>(nav_.top())) nav_.pop(ui);
    nav_.push(ui, std::make_unique<ui::PlayerScreen>(s_, target));
}

void Router::open_extensions(wnk::ui::Ui &ui) { nav_.push(ui, std::make_unique<ui::ExtensionsScreen>(s_)); }

void Router::open_provider_tests(wnk::ui::Ui &ui) { nav_.push(ui, std::make_unique<ui::ProviderTestScreen>(s_)); }

void Router::open_profiles(wnk::ui::Ui &ui, bool startup, std::function<void(wnk::ui::Ui &)> then) {
    nav_.push(ui, std::make_unique<ui::ProfilesScreen>(s_, startup, std::move(then)));
}

void Router::switch_profile(wnk::ui::Ui &ui, int key_index, bool back_home) {
    if (!s_.profiles.select(key_index)) return;
    s_.settings->set(settings::kProfiles, s_.profiles.to_json());
    s_.settings->set_profile(key_index);
    s_.library->switch_profile(key_index);
    if (back_home) nav_.pop_to_root(ui);
    s_.profile_changed.emit();  // Home loads the profile's source, the top bars show it
    if (back_home) ui.toast("Switched to " + s_.profiles.current().name);
}

void Router::open_repository(wnk::ui::Ui &ui, const plugins::RepositoryData &repo) {
    nav_.push(ui, std::make_unique<ui::RepositoryScreen>(s_, repo));
}

void Router::back(wnk::ui::Ui &ui) { nav_.pop(ui); }

}  // namespace csweb::app
