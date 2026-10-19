// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/episode_row.hpp"

#include "ui/theme/theme.hpp"

namespace csweb::ui {

using namespace wnk::ui;

EpisodeRow::EpisodeRow(const runtime::Episode &ep, std::string fallback_image, ClickHandler on_click)
    : on_click_(std::move(on_click)) {
    set_focusable(true);
    set_focus_ring(true, 20);
    radius_ = 20;
    still_ = &add<ImageView>(ep.poster_url.empty() ? std::move(fallback_image) : ep.poster_url);
    still_->set_background(theme::kPlaceholder, 12);
    std::string head = ep.episode > 0 ? "E" + std::to_string(ep.episode) + "   " : std::string();
    head += ep.name.empty() ? (ep.episode > 0 ? "Episode " + std::to_string(ep.episode) : "Episode") : ep.name;
    title_ = &add<Label>(head);
    title_->set_bold(true).set_font_size(theme::kBody);
    std::string meta;
    if (ep.season > 0) meta += "Season " + std::to_string(ep.season);
    if (ep.run_time > 0) meta += std::string(meta.empty() ? "" : "  ·  ") + std::to_string(ep.run_time) + " min";
    if (!ep.score.empty() && ep.score != "null") meta += std::string(meta.empty() ? "" : "  ·  ") + "★ " + ep.score;
    if (!ep.dub.empty()) meta += std::string(meta.empty() ? "" : "  ·  ") + ep.dub;
    meta_ = &add<Label>(meta, TextRole::Small);
    meta_->set_muted(true);
    plot_ = &add<Label>(ep.description == "null" ? std::string() : ep.description, TextRole::Small);
    plot_->set_color(theme::kTextSecondary).set_max_lines(2);
}

std::string EpisodeRow::accessible_text() const { return title_->text(); }

void EpisodeRow::layout_content(Ui &ui, const wnk::Rect &c) {
    const float pad = 14, still_w = 300, still_h = 169;
    still_->layout(ui, {c.x + pad, c.y + (c.h - still_h) * 0.5f, still_w, still_h});
    const float tx = c.x + pad + still_w + 26, tw = c.w - (tx - c.x) - pad;
    float y = c.y + 22;
    const wnk::Size t = title_->measure(ui, {tw, 60});
    title_->layout(ui, {tx, y, tw, t.h});
    y += t.h + 6;
    const wnk::Size m = meta_->measure(ui, {tw, 40});
    meta_->layout(ui, {tx, y, tw, m.h});
    y += m.h + 8;
    const wnk::Size p = plot_->measure(ui, {tw, 80});
    plot_->layout(ui, {tx, y, tw, p.h});
}

bool EpisodeRow::on_action(Ui &ui, const wnk::input::ActionEvent &e) {
    if (e.action != wnk::input::Action::Confirm || !on_click_) return false;
    if (e.phase == wnk::input::Phase::Press) on_click_(ui);
    return true;
}

void EpisodeRow::draw(Ui &ui) {
    if (ui.is_focused(this)) ui.renderer().fill_rect(frame(), ui.fade(theme::kSurfaceRaised), radius_);
}

}  // namespace csweb::ui
