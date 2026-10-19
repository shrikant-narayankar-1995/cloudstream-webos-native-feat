// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/search_screen.hpp"

#include "core/guard.hpp"
#include "ui/theme/theme.hpp"
#include "ui/widgets/pill_button.hpp"
#include "ui/widgets/top_bar.hpp"

#include <algorithm>

namespace csweb::ui {

using namespace wnk::ui;

namespace {
constexpr const char *kHistoryKey = "search_history";
constexpr size_t kHistoryMax = 10;
const wnk::Size kCell{theme::kPosterW, theme::kPosterH + 56};
}  // namespace

SearchScreen::SearchScreen(app::Services &s) : Screen("Search"), s_(s) {
    name = "search";
    auto &body = add<wnk::ui::Row>(56);
    body.set_padding({theme::kSafeX, 170, 0, 40});

    auto &left = body.add<Column>(22);
    left.set_width(Dim::fixed(560));
    left.add<Label>("Search all sources", TextRole::Small).set_muted(true);
    field_ = &left.add<TextField>("Title, actor…");
    field_->name = "search.field";
    field_->set_max_length(120);
    field_->set_on_submit([this](Ui &ui, const std::string &q) { search(ui, q); });
    left.add<Label>("Recent", TextRole::Small).set_muted(true).set_padding({0, 20, 0, 0});
    history_ = &left.add<wnk::ui::Row>(10);
    show_history();

    auto &scroll = body.add<ScrollView>();
    scroll.set_width(Dim::fill());
    scroll.margin = 120;
    results_ = &scroll.add<Column>(22);
    results_->set_padding({0, 0, theme::kSafeX, 60});
    status_ = &results_->add<Label>("Results from every installed source appear here, grouped by source.");
    status_->set_muted(true).set_max_lines(2);

    add<TopBar>(*s_.router, app::Router::Tab::Search);
}

void SearchScreen::on_show(Ui &ui) {
    Screen::on_show(ui);
    if (!shown_) ui.focus(field_);  // opened from the nav bar: straight to typing
    shown_ = true;
}

SearchScreen::~SearchScreen() { cancel(); }

void SearchScreen::cancel() {
    for (auto &kv : running_) s_.runtime->cancel(kv.first);
    running_.clear();
}

void SearchScreen::show_history() {
    history_->clear_children();
    auto it = s_.settings->get_string(kHistoryKey);
    const auto list = nlohmann::json::parse(it.empty() ? "[]" : it, nullptr, false);
    if (!list.is_array()) return;
    size_t n = 0;
    for (const auto &q : list) {
        if (!q.is_string() || n++ >= 4) continue;  // the row shows the four most recent
        const std::string query = q.get<std::string>();
        history_->add<PillButton>(query, "", PillButton::Style::Glass, [this, query](Ui &ui) {
            field_->set_text(query);
            search(ui, query);
        }, 52);
    }
}

void SearchScreen::update_status() {
    std::string text;
    if (!running_.empty()) {
        std::string names;
        for (auto &kv : running_) names += (names.empty() ? "" : ", ") + kv.second;
        text = "Still searching: " + names;
    }
    if (!failed_.empty()) {
        std::string names;
        for (auto &f : failed_) names += (names.empty() ? "" : ", ") + f;
        text += std::string(text.empty() ? "" : "  ·  ") + "No answer from: " + names;
    }
    if (running_.empty() && groups_.empty() && !query_.empty()) text = "Nothing found for “" + query_ + "”.";
    status_->set_text(text);
}

void SearchScreen::search(Ui &, const std::string &raw) {
    std::string q = raw;
    while (!q.empty() && q.back() == ' ') q.pop_back();
    if (q.empty()) return;
    cancel();
    query_ = q;
    failed_.clear();
    for (auto &g : groups_) {
        results_->remove_child(g->title);
        results_->remove_child(g->rail);
    }
    groups_.clear();

    // History: most recent first, no duplicates (SearchHistoryAdaptor).
    auto hist = nlohmann::json::parse(s_.settings->get_string(kHistoryKey, "[]"), nullptr, false);
    nlohmann::json next = nlohmann::json::array({q});
    if (hist.is_array()) {
        for (const auto &h : hist) {
            if (h.is_string() && h.get<std::string>() != q && next.size() < kHistoryMax) next.push_back(h);
        }
    }
    s_.settings->set(kHistoryKey, next.dump());
    show_history();

    for (const auto &p : s_.plugins->providers()) {
        const std::string provider = p.name;
        auto items = std::make_shared<std::vector<runtime::SearchResult>>();
        auto empty = std::make_shared<bool>(false);  // answered with no results (the runtime's "empty")
        auto id = std::make_shared<runtime::RuntimeClient::RequestId>(0);
        *id = s_.runtime->request(
            {{"op", "search"}, {"provider", provider}, {"query", q}, {"limit", 60}},
            [items, empty](const nlohmann::json &e) {
                const std::string ev = e.value("event", "");
                if (ev == "result") items->push_back(runtime::parse_search_result(e));
                else if (ev == "empty") *empty = true;
            },
            core::guarded(life_.weak(), [this, provider, items, empty, id, q](bool ok, const std::string &) {
                running_.erase(*id);
                if (q != query_) return;
                if (!ok && items->empty() && !*empty) failed_.push_back(provider);
                if (!items->empty()) {
                    auto g = std::make_unique<Group>();
                    g->provider = provider;
                    g->items = std::move(*items);
                    g->title = &results_->add<Label>(provider + "   " + std::to_string(g->items.size()) + " results");
                    g->title->set_bold(true).set_font_size(theme::kHeadline);
                    g->rail = &results_->add<CollectionView>(Axis::Horizontal, 1, kCell, 24);
                    g->rail->set_width(Dim::fill()).set_height(Dim::fixed(kCell.h + 40));
                    Group *raw = g.get();
                    g->rail->set_items(raw->items.size(), [raw](Ui &, size_t i) {
                        const auto &it = raw->items[i];
                        auto card = std::make_unique<Card>(it.name, it.poster_url, it.year > 0 ? std::to_string(it.year) : std::string());
                        card->image_aspect = 1.5f;
                        return card;
                    });
                    g->rail->on_activate = [this, raw](Ui &ui, size_t i) {
                        if (i < raw->items.size()) s_.router->open_details(ui, raw->provider, raw->items[i]);
                    };
                    groups_.push_back(std::move(g));
                }
                update_status();
            }));
        if (*id != 0) running_[*id] = provider;
    }
    if (s_.plugins->providers().empty()) {
        status_->set_text("No sources are loaded. Install plugins in Settings › Extensions.");
        return;
    }
    update_status();
}

}  // namespace csweb::ui
