// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/splash_screen.hpp"

#include "ui/theme/theme.hpp"
#include "ui/widgets/icons.hpp"

#include <algorithm>
#include <cmath>

namespace csweb::ui {

using namespace wnk::ui;

namespace {
// The "webOS" label of the launcher icon: white on the cloud's blue.
class WebOsLabel : public View {
 protected:
    wnk::Size measure_content(Ui &, wnk::Size) override { return {132, 44}; }
    void draw(Ui &ui) override {
        auto &r = ui.renderer();
        const wnk::Rect f = frame();
        r.fill_rect(f, ui.fade(theme::Color::hex(0x254CDBFF)), f.h * 0.5f);
        const wnk::render::TextStyle s{ui.theme().bold, 26, ui.fade(theme::kText)};
        const float w = r.measure_text(s, "webOS");
        r.draw_text(s, {f.x + (f.w - w) * 0.5f, f.y + (f.h - r.line_height(s)) * 0.5f + r.ascent(s)}, "webOS");
    }
};
}  // namespace

SplashScreen::SplashScreen(std::function<void(Ui &)> done) : Screen("CloudStream"), done_(std::move(done)) {
    name = "splash";
    set_background(theme::kBackground);
    auto &col = add<Column>(30);
    col.set_width(Dim::fill()).set_height(Dim::fill());
    col.set_justify(Align::Center);
    auto &logo = col.add<ImageView>(asset_url("brand/logo.png"));
    logo.set_fit(ImageFit::Contain);
    logo.set_size(360, 171);  // the cloud is 512 x 243
    logo.set_align(Align::Center, Align::Start);
    auto &title = col.add<Label>("CloudStream", TextRole::Title);
    title.set_font_size(theme::kTitle).set_bold(true);
    title.set_align(Align::Center, Align::Start);
    col.add<WebOsLabel>().set_align(Align::Center, Align::Start);
}

bool SplashScreen::on_back(Ui &) { return true; }

void SplashScreen::update(Ui &ui, float dt) {
    Screen::update(ui, dt);
    if (!done_) return;
    elapsed_ += std::min(dt, 0.1f);  // the first frames carry the start-up time
    if (elapsed_ >= kSeconds) {
        auto done = std::move(done_);
        done_ = nullptr;
        done(ui);
    }
}

void SplashScreen::draw_over(Ui &ui) {
    // Three dots pulsing in turn under the label: the app is starting.
    auto &r = ui.renderer();
    const wnk::Rect f = frame();
    const float t = static_cast<float>(ui.now_ms()) / 1000.0f;
    const float y = f.y + f.h - 150;
    for (int i = 0; i < 3; i++) {
        const float a = 0.25f + 0.75f * (0.5f + 0.5f * std::sin(t * 5.0f - i * 0.9f));
        r.fill_rect({f.x + f.w * 0.5f - 46 + i * 36.0f, y, 20, 20}, ui.fade(theme::kAccent.with_alpha(a)), 10);
    }
}

}  // namespace csweb::ui
