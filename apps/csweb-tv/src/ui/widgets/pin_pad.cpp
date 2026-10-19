// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/pin_pad.hpp"

#include "ui/theme/theme.hpp"

#include <algorithm>

namespace csweb::ui {

using namespace wnk::ui;

namespace {
constexpr size_t kDigits = 4;

// A digit key: a rounded square, white when focused (design 13).
class Key : public Button {
 public:
    Key(std::string text, ClickHandler on_click) : Button(std::move(text), std::move(on_click)) {
        set_focus_ring(false);
        set_padding({});
    }

 protected:
    wnk::Size measure_content(Ui &ui, wnk::Size) override {
        const wnk::render::TextStyle s{ui.theme().bold, 32, theme::kText};
        return {std::max(96.0f, ui.renderer().measure_text(s, text()) + 48), 80};
    }
    void draw(Ui &ui) override {
        auto &r = ui.renderer();
        const wnk::Rect f = frame();
        const bool focused = ui.is_focused(this);
        r.fill_rect(f, ui.fade(focused ? theme::kFocusFill : theme::Color::hex(0xFFFFFF14)), 16);
        const wnk::render::TextStyle s{ui.theme().bold, 32, ui.fade(focused ? theme::kOnFocus : theme::kText)};
        const float w = r.measure_text(s, text());
        r.draw_text(s, {f.x + (f.w - w) * 0.5f, f.y + (f.h - r.line_height(s)) * 0.5f + r.ascent(s)}, text());
    }
};

class Dots : public View {
 public:
    explicit Dots(const std::string &entered) : entered_(entered) {}

 protected:
    wnk::Size measure_content(Ui &, wnk::Size) override { return {kDigits * 28 + (kDigits - 1) * 22.0f, 28}; }
    void draw(Ui &ui) override {
        auto &r = ui.renderer();
        const wnk::Rect f = frame();
        for (size_t i = 0; i < kDigits; i++) {
            const wnk::Rect d{f.x + i * 50.0f, f.y, 28, 28};
            if (i < entered_.size()) {
                r.fill_rect(d, ui.fade(theme::kFocusFill), 14);
            } else {
                r.stroke_rect(d, ui.fade(theme::Color::hex(0xFFFFFF80)), 3, 14);
            }
        }
    }

 private:
    const std::string &entered_;
};
}  // namespace

PinPad::PinPad(Submit on_submit) : Column(24), on_submit_(std::move(on_submit)) {
    prompt_ = &add<Label>("");
    prompt_->set_color(theme::kTextSecondary).set_font_size(theme::kLabel);
    prompt_->set_align(Align::Center, Align::Start);
    dots_ = &add<Dots>(entered_);
    dots_->set_align(Align::Center, Align::Start);
    auto &keys = add<Row>(12);
    keys.set_align(Align::Center, Align::Start);
    keys.set_padding({0, 8, 0, 0});
    keys.set_focus_scope();
    for (char c : std::string("1234567890")) {
        auto &k = keys.add<Key>(std::string(1, c), [this, c](Ui &ui) { press(ui, c); });
        k.name = std::string("pin.") + c;
        k.accessible_label = std::string(1, c);
        if (first_ == nullptr) first_ = &k;
    }
    auto &del = keys.add<Key>("Delete", [this](Ui &) { backspace(); });
    del.name = "pin.delete";
    del.accessible_label = "Delete";
    hint_ = &add<Label>("", TextRole::Small);
    hint_->set_muted(true).set_font_size(22);
    hint_->set_align(Align::Center, Align::Start);
}

void PinPad::set_prompt(const std::string &text) { prompt_->set_text(text); }

void PinPad::clear() {
    entered_.clear();
    hint_->set_text("");
    hint_->set_color(theme::kTextMuted);
}

void PinPad::show_error(const std::string &text) {
    entered_.clear();
    hint_->set_text(text);
    hint_->set_color(theme::kWarn);
}

void PinPad::press(Ui &ui, char digit) {
    if (entered_.size() >= kDigits) return;
    entered_.push_back(digit);
    if (entered_.size() == kDigits && on_submit_) {
        const std::string pin = entered_;
        on_submit_(ui, pin);  // may destroy this pad
    }
}

void PinPad::backspace() {
    if (!entered_.empty()) entered_.pop_back();
}

}  // namespace csweb::ui
