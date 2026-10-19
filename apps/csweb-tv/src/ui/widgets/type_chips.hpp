// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream's TvType chips (tvtypes_chips: Movies, TV Series, Anime, …) as a grid of pills: only
// the chips some provider supports are shown, each toggles its types, and the handler gets every
// checked chip's types (HomeFragment.bindChips).
#pragma once

#include "runtime/runtime_types.hpp"

#include "wnk/ui/widgets.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace csweb::ui {

class TypeChips : public wnk::ui::Grid {
 public:
    using Handler = std::function<void(wnk::ui::Ui &, const std::set<std::string> &)>;
    TypeChips(int columns, Handler on_change);
    // Rebuilds the chips for these providers with `selected` checked.
    void set(const std::vector<runtime::Provider> &providers, const std::set<std::string> &selected);
    const std::set<std::string> &selected() const { return selected_; }

 private:
    void refresh();
    Handler on_change_;
    std::set<std::string> selected_;
};

}  // namespace csweb::ui
