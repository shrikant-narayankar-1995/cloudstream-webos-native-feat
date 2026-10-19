// SPDX-License-Identifier: GPL-3.0-or-later
//
// Artwork with the legibility scrims of the designs: a backdrop image filling the view, darkened
// towards the left (where hero text sits) and towards the bottom (where rows start). The renderer
// has no gradients, so each ramp is a stack of thin bands.
#pragma once

#include "wnk/ui/widgets.hpp"

#include <string>

namespace csweb::ui {

class Backdrop : public wnk::ui::View {
 public:
    explicit Backdrop(std::string url = {});
    void set_url(const std::string &url);
    float left_strength = 0.92f;    // alpha at the left edge
    float left_extent = 0.75f;      // fraction of the width the left ramp covers
    float bottom_extent = 0.5f;     // fraction of the height the bottom ramp covers
    bool fade_to_background = true;  // bottom ramp ends in the screen background colour

 protected:
    void draw_over(wnk::ui::Ui &ui) override;

 private:
    wnk::ui::ImageView *image_;
};

// Fills `r` with `steps` bands from alpha a0 to a1 along x (horizontal) or y.
void draw_ramp(wnk::ui::Ui &ui, const wnk::Rect &r, wnk::render::Color c, float a0, float a1, bool horizontal, int steps = 24);

}  // namespace csweb::ui
