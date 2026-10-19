// SPDX-License-Identifier: GPL-3.0-or-later
//
// Test extensions (CloudStream's TestFragment): every loaded provider goes through the home page,
// search, load and loadLinks with its plugin's own code (runtime/provider_test.hpp), all at once.
// The header counts passed and failed providers; a provider's row opens its log. Start runs the tests
// again, Stop cancels them.
#pragma once

#include "app/router.hpp"
#include "runtime/provider_test.hpp"

#include "wnk/ui/navigation.hpp"

#include <memory>
#include <vector>

namespace wnk::ui {
class ValueRow;
}

namespace csweb::ui {

class PillButton;

class ProviderTestScreen : public wnk::ui::Screen {
 public:
    explicit ProviderTestScreen(app::Services &s);
    ~ProviderTestScreen() override;
    void on_show(wnk::ui::Ui &ui) override;

 private:
    void start();
    void stop();
    void refresh();
    void show_log(wnk::ui::Ui &ui, size_t index);

    app::Services &s_;
    std::vector<std::unique_ptr<runtime::ProviderTest>> tests_;
    std::vector<wnk::ui::ValueRow *> rows_;
    wnk::ui::Label *summary_ = nullptr;
    wnk::ui::Column *list_ = nullptr;
    PillButton *start_ = nullptr;
    bool started_ = false;
    bool reported_ = false;  // results logged once all have finished
};

}  // namespace csweb::ui
