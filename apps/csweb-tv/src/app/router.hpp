// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens screens. Screens never construct each other directly: they ask the router, which owns the
// navigation stack, so the flow (Home → Details → Sources → Player, Back to where you were) is in one
// place and matches CloudStream's (MainActivity navigation + ResultFragment + GeneratorPlayer).
// The main tabs replace each other above Home, as CloudStream's bottom navigation does: Back from a
// tab returns to Home, Back on Home leaves the app.
#pragma once

#include "app/services.hpp"
#include "library/library.hpp"
#include "runtime/runtime_types.hpp"

#include "wnk/ui/navigation.hpp"

#include <functional>
#include <string>
#include <vector>

namespace csweb::app {

// What to play: the provider and the data for loadLinks (an episode's data or a movie's dataUrl),
// the show it belongs to (for resume and bookmarks) and the episodes after it, in order (the player
// plays the next one when this one ends, and resume moves to it past 90 %). `episodes` is the whole
// list this one is in (GeneratorPlayer's allMeta), for the player's Episodes overlay.
struct PlayTarget {
    library::Show show;
    std::vector<library::EpisodeRef> queue;
    std::vector<library::EpisodeRef> episodes;
    size_t index = 0;  // this one in `episodes`
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

    enum class Tab { Home, Search, Library, Settings };
    void open_tab(wnk::ui::Ui &ui, Tab tab);
    void open_search(wnk::ui::Ui &ui, const std::string &query);
    // `resume`: play the show's resume episode once the details have loaded (Continue watching).
    void open_details(wnk::ui::Ui &ui, const std::string &provider, const runtime::SearchResult &item, bool resume = false);
    // The player, which loads the target's links itself (GeneratorPlayer).
    void open_sources(wnk::ui::Ui &ui, const PlayTarget &target);
    // From the player: replaces it with a player for `target` (the next episode).
    void replace_player(wnk::ui::Ui &ui, const PlayTarget &target);
    void open_extensions(wnk::ui::Ui &ui);
    void open_provider_tests(wnk::ui::Ui &ui);
    // The profile picker; at start (`startup`) Back leaves the app and `then` runs once one is chosen.
    void open_profiles(wnk::ui::Ui &ui, bool startup = false, std::function<void(wnk::ui::Ui &)> then = {});
    // Makes `key_index` the current profile: its settings and library, Home reloaded (setAccount).
    // `back_home`: show Home afterwards (CloudStream returns to MainActivity).
    void switch_profile(wnk::ui::Ui &ui, int key_index, bool back_home = true);
    Services &services() { return s_; }
    void open_repository(wnk::ui::Ui &ui, const plugins::RepositoryData &repo);
    void back(wnk::ui::Ui &ui);

    wnk::ui::Navigator &nav() { return nav_; }

 private:
    Services &s_;
    wnk::ui::Navigator &nav_;
};

}  // namespace csweb::app
