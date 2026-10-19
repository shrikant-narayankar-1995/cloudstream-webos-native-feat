// SPDX-License-Identifier: GPL-3.0-or-later
//
// Extensions (design 10), like CloudStream's ExtensionsFragment: the repositories added, with how
// many of their plugins are installed; "Add repository" takes a URL, a cloudstreamrepo:// link or a
// shortcode (RepositoryManager.parseRepoUrl); a repository opens its plugin list; removing one
// removes its plugins too. The app ships no repositories and no plugins.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

namespace csweb::ui {

class ExtensionsScreen : public wnk::ui::Screen {
 public:
    explicit ExtensionsScreen(app::Services &s);
    void on_show(wnk::ui::Ui &ui) override;

 private:
    void rebuild();
    void open_add(wnk::ui::Ui &ui);
    void confirm_remove(wnk::ui::Ui &ui, const plugins::RepositoryData &repo);

    app::Services &s_;
    wnk::ui::Column *list_ = nullptr;
    wnk::ui::Label *summary_ = nullptr;
    core::Signal::Subscription sub_;
    wnk::async::Lifetime life_;
};

// One repository's plugins (CloudStream's PluginsFragment): install, update, remove, settings.
class RepositoryScreen : public wnk::ui::Screen {
 public:
    RepositoryScreen(app::Services &s, plugins::RepositoryData repo);
    void on_show(wnk::ui::Ui &ui) override;

 private:
    void fetch();
    void rebuild();
    void open_settings(wnk::ui::Ui &ui, const plugins::InstalledPlugin &p);

    app::Services &s_;
    plugins::RepositoryData repo_;
    std::vector<plugins::SitePlugin> plugins_;
    std::string error_;
    bool fetching_ = false;
    std::string busy_;  // plugin being installed or removed
    wnk::ui::Column *list_ = nullptr;
    wnk::ui::Label *status_ = nullptr;
    core::Signal::Subscription sub_;
    wnk::async::Lifetime life_;
};

}  // namespace csweb::ui
