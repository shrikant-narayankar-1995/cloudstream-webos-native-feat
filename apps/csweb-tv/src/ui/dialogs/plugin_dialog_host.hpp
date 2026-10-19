// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shows the dialogs plugins open (their settings, mostly) with the kit's widgets and sends back what
// the viewer did (docs/runtime/dialog-protocol.md). The plugin's own Android code runs unchanged in
// the runtime; this only draws the description and answers it. A dialog that stays open after an
// answer is described again and redrawn. Field values and answers are never logged.
#pragma once

#include "runtime/runtime_client.hpp"

#include "wnk/ui/navigation.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

namespace csweb::ui {

class PluginDialogHost {
 public:
    explicit PluginDialogHost(runtime::RuntimeClient &runtime) : runtime_(runtime) {}
    // dialog, dialog_closed and toast events (RuntimeClient::on_dialog).
    void handle(wnk::ui::Ui &ui, const nlohmann::json &event);

 private:
    struct Field {
        std::string key;
        std::function<nlohmann::json()> value;
        std::function<bool()> changed;  // a password field is sent only when typed into
    };
    void show(wnk::ui::Ui &ui, const nlohmann::json &d);
    void build(wnk::ui::View &parent, const nlohmann::json &view, int dialog);
    void answer(int dialog, nlohmann::json extra);
    void close_current(wnk::ui::Ui &ui);

    runtime::RuntimeClient &runtime_;
    wnk::ui::Dialog *current_ = nullptr;
    int current_id_ = 0;
    bool answered_ = false;  // the shown dialog was answered: its dismissal is not a cancel
    std::vector<Field> fields_;
};

}  // namespace csweb::ui
