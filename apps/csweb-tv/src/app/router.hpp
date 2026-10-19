// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens screens. Screens never construct each other directly: they ask the router, which owns the
// navigation stack, so the flow (Home → Details → Sources → Player, Back to where you were) is in one
// place and matches CloudStream's (MainActivity navigation + ResultFragment + GeneratorPlayer).
// The main tabs replace each other above Home, as CloudStream's bottom navigation does: Back from a
// tab returns to Home, Back on Home leaves the app.
#pragma once

#include "app/services.hpp"
#include "runtime/runtime_types.hpp"

#include "wnk/ui/navigation.hpp"

#include <string>
#include <vector>

namespace csweb::app {

// What to play: the provider and the data for loadLinks (an episode's data or a movie's dataUrl).
struct PlayTarget {
    std::string provider;
    std::string data;
    std::string title;      // the show or movie
    std::string subtitle;   // "S2 · E4 Low Tide", empty for a movie
    std::string poster_url;
    std::string details_url;  // the LoadResponse url, the key for resume and history
    int season = 0, episode = 0;
};

class Router {
 public:
    Router(Services &services, wnk::ui::Navigator &nav);

    enum class Tab { Home, Search, Library, Downloads, Settings };
    void open_tab(wnk::ui::Ui &ui, Tab tab);
    void open_details(wnk::ui::Ui &ui, const std::string &provider, const runtime::SearchResult &item);
    // The player, which loads the target's links itself (GeneratorPlayer).
    void open_sources(wnk::ui::Ui &ui, const PlayTarget &target);
    void open_extensions(wnk::ui::Ui &ui);
    void open_repository(wnk::ui::Ui &ui, const plugins::RepositoryData &repo);
    void back(wnk::ui::Ui &ui);

    wnk::ui::Navigator &nav() { return nav_; }

 private:
    Services &s_;
    wnk::ui::Navigator &nav_;
};

}  // namespace csweb::app
