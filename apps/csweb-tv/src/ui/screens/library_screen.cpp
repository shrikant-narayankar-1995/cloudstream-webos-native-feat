// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/library_screen.hpp"

#include "ui/theme/theme.hpp"
#include "ui/widgets/top_bar.hpp"

namespace csweb::ui {

using namespace wnk::ui;

namespace {
constexpr int kColumns = 6;
std::vector<std::string> tab_titles(const library::Library &lib) {
    std::vector<std::string> out;
    for (library::WatchType t : library::watch_types()) {
        const size_t n = lib.bookmarks(t).size();
        out.push_back(std::string(library::watch_type_label(t)) + (n ? "  " + std::to_string(n) : std::string()));
    }
    return out;
}
}  // namespace

LibraryScreen::LibraryScreen(app::Services &s) : Screen("Library"), s_(s) {
    name = "library";
    auto &col = add<Column>(24);
    col.set_padding({theme::kSafeX, 170, theme::kSafeX, 40});
    col.set_width(Dim::fill()).set_height(Dim::fill());
    tabs_ = &col.add<Tabs>(tab_titles(s_.library->get()), 0, [this](Ui &, int i) {
        tab_ = i;
        rebuild();
    });
    tabs_->set_focus_scope();
    empty_ = &col.add<Label>("");
    empty_->set_muted(true).set_max_lines(2);
    grid_ = &col.add<CollectionView>(Axis::Vertical, kColumns, wnk::Size{theme::kPosterW, theme::kPosterH + 56}, 28);
    grid_->set_width(Dim::fill()).set_height(Dim::fill());
    grid_->name = "library.grid";
    grid_->on_activate = [this](Ui &ui, size_t i) {
        if (i >= shown_.size()) return;
        const auto &sh = shown_[i].show;
        runtime::SearchResult item;
        item.name = sh.name;
        item.url = sh.url;
        item.api_name = sh.provider;
        item.type = sh.type;
        item.poster_url = sh.poster_url;
        item.year = sh.year;
        s_.router->open_details(ui, sh.provider, item);
    };
    add<TopBar>(*s_.router, app::Router::Tab::Library);
    sub_ = s_.library->changed.subscribe([this] { rebuild(); });
    rebuild();
}

void LibraryScreen::on_show(Ui &ui) {
    Screen::on_show(ui);
    if (!shown_once_) ui.focus_within(tabs_);
    shown_once_ = true;
}

void LibraryScreen::rebuild() {
    const auto &lib = s_.library->get();
    const auto titles = tab_titles(lib);
    for (size_t i = 0; i < titles.size() && i < tabs_->count(); i++) {
        if (auto *b = dynamic_cast<Button *>(&tabs_->tab(i))) b->set_text(titles[i]);
    }
    const library::WatchType type = library::watch_types()[static_cast<size_t>(tab_)];
    shown_ = lib.bookmarks(type);
    // Shows whose plugin is not loaded still open: Details reports what the provider answers.
    empty_->set_text(shown_.empty() ? std::string("Nothing in ") + library::watch_type_label(type) +
                                          " yet. Use the bookmark button on a show's page."
                                    : "");
    empty_->set_visible(shown_.empty());
    grid_->set_items(shown_.size(), [this](Ui &, size_t i) {
        const auto &sh = shown_[i].show;
        return std::make_unique<Card>(sh.name, sh.poster_url, sh.provider);
    });
}

}  // namespace csweb::ui
