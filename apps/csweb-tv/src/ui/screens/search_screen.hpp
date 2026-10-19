// SPDX-License-Identifier: GPL-3.0-or-later
//
// Search (design 02), like CloudStream's SearchFragment/SearchViewModel: the query goes to every
// loaded provider at once, results are grouped by provider as each answers, providers still
// searching and the ones that failed are named, and recent queries are kept (search history).
// Typing uses LG's on-screen keyboard through the kit's TextField.
#pragma once

#include "app/router.hpp"
#include "runtime/runtime_types.hpp"

#include "wnk/ui/collection.hpp"
#include "wnk/ui/navigation.hpp"

#include <map>
#include <string>
#include <vector>

namespace csweb::ui {

class SearchScreen : public wnk::ui::Screen {
 public:
    explicit SearchScreen(app::Services &s);
    ~SearchScreen() override;
    void on_show(wnk::ui::Ui &ui) override;
    // A query from outside (a cloudstreamsearch:// link): typed into the field and searched.
    void start_query(wnk::ui::Ui &ui, const std::string &query);

 private:
    struct Group {
        std::string provider;
        std::vector<runtime::SearchResult> items;
        wnk::ui::Label *title = nullptr;
        wnk::ui::CollectionView *rail = nullptr;
    };
    void search(wnk::ui::Ui &ui, const std::string &query);
    void show_history();
    void update_status();
    void cancel();

    app::Services &s_;
    wnk::ui::TextField *field_ = nullptr;
    wnk::ui::Row *history_ = nullptr;
    wnk::ui::Column *results_ = nullptr;
    wnk::ui::Label *status_ = nullptr;
    std::vector<std::unique_ptr<Group>> groups_;
    std::map<runtime::RuntimeClient::RequestId, std::string> running_;
    std::vector<std::string> failed_;
    std::string query_;
    bool shown_ = false;
    wnk::async::Lifetime life_;
};

}  // namespace csweb::ui
