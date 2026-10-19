// SPDX-License-Identifier: GPL-3.0-or-later
//
// Library (design 07), like CloudStream's LibraryFragment for the local lists: one tab per WatchType
// (Watching, Completed, On hold, Dropped, Plan to watch) with the bookmarked shows, most recently
// changed first; choosing one opens its details. Sort orders the shows as the local list can
// (library_sorting_mode, per profile). Account sync (AniList, MAL…) is not built.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/collection.hpp"
#include "wnk/ui/forms.hpp"
#include "wnk/ui/navigation.hpp"

namespace csweb::ui {

class LibraryScreen : public wnk::ui::Screen {
 public:
    explicit LibraryScreen(app::Services &s);
    void on_show(wnk::ui::Ui &ui) override;

 private:
    void rebuild();
    void choose_sorting(wnk::ui::Ui &ui);
    library::ListSorting sorting() const;
    app::Services &s_;
    int tab_ = 0;
    std::vector<library::Bookmark> shown_;
    wnk::ui::Tabs *tabs_ = nullptr;
    wnk::ui::Button *sort_ = nullptr;
    wnk::ui::CollectionView *grid_ = nullptr;
    wnk::ui::Label *empty_ = nullptr;
    bool shown_once_ = false;
    core::Signal::Subscription sub_;
};

}  // namespace csweb::ui
