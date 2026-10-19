// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/type_chips.hpp"

#include "settings/preferences.hpp"
#include "ui/widgets/pill_button.hpp"

namespace csweb::ui {

using namespace wnk::ui;

TypeChips::TypeChips(int columns, Handler on_change) : Grid(columns, 10), on_change_(std::move(on_change)) {
    set_focus_scope();
}

void TypeChips::set(const std::vector<runtime::Provider> &providers, const std::set<std::string> &selected) {
    selected_ = selected;
    clear_children();
    for (const auto &chip : settings::type_chips()) {
        if (!settings::chip_visible(chip, providers)) continue;
        const std::string label = chip.label;
        auto &b = add<PillButton>(label, "", PillButton::Style::Glass, [this, label](Ui &ui) {
            for (const auto &c : settings::type_chips()) {
                if (c.label != label) continue;
                selected_ = settings::toggle_chip(c, selected_);
                refresh();
                if (on_change_) on_change_(ui, selected_);
                return;
            }
        }, 52);
        b.name = "chip." + label;
    }
    refresh();
}

void TypeChips::refresh() {
    for (auto &child : children()) {
        auto *b = static_cast<PillButton *>(child.get());
        for (const auto &c : settings::type_chips()) {
            if (c.label == b->text()) {
                const bool on = settings::chip_selected(c, selected_);
                b->set_selected(on);
                b->set_icon(on ? "check" : "");
            }
        }
    }
}

}  // namespace csweb::ui
