// SPDX-License-Identifier: GPL-3.0-or-later
//
// The launch splash, as CloudStream's (splash_background: the launcher cloud centred on the app's
// background): the cloud, the name and a "webOS" label, for a moment while the app starts behind
// it; then `done` runs (it pops the splash, which fades out over the next screen). Keys do nothing
// meanwhile; Back is kept.
#pragma once

#include "app/router.hpp"

#include "wnk/ui/navigation.hpp"

#include <functional>

namespace csweb::ui {

class SplashScreen : public wnk::ui::Screen {
 public:
    static constexpr float kSeconds = 1.3f;
    explicit SplashScreen(std::function<void(wnk::ui::Ui &)> done);
    bool on_back(wnk::ui::Ui &ui) override;

 protected:
    void update(wnk::ui::Ui &ui, float dt) override;
    void draw_over(wnk::ui::Ui &ui) override;

 private:
    std::function<void(wnk::ui::Ui &)> done_;
    float elapsed_ = 0;
};

}  // namespace csweb::ui
