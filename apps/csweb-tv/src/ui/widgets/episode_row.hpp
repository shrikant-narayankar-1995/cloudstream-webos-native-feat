// SPDX-License-Identifier: GPL-3.0-or-later
//
// One episode in a list (design 04): a 16:9 still, "E4" and the name, a meta line and the
// synopsis. Focus fills the row and rings it.
#pragma once

#include "runtime/runtime_types.hpp"

#include "wnk/ui/widgets.hpp"

namespace csweb::ui {

class EpisodeRow : public wnk::ui::View {
 public:
    EpisodeRow(const runtime::Episode &ep, std::string fallback_image, wnk::ui::ClickHandler on_click);
    std::string accessible_text() const override;

 protected:
    void layout_content(wnk::ui::Ui &ui, const wnk::Rect &content) override;
    bool on_action(wnk::ui::Ui &ui, const wnk::input::ActionEvent &e) override;
    void draw(wnk::ui::Ui &ui) override;

 private:
    wnk::ui::ImageView *still_;
    wnk::ui::Label *title_, *meta_, *plot_;
    wnk::ui::ClickHandler on_click_;
};

}  // namespace csweb::ui
