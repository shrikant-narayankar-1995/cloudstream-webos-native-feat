// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/theme/theme.hpp"

namespace csweb::ui::theme {

wnk::ui::Theme make_kit_theme() {
    wnk::ui::Theme t;
    t.background = kBackground;
    t.surface = Color::hex(0x161A22FF);
    t.raised = Color::hex(0x222734FF);
    t.raised_focused = Color::hex(0x3A4258FF);
    t.text = kText;
    t.text_muted = kTextMuted;
    t.text_disabled = Color::hex(0x5D6474FF);
    t.accent = kAccent;
    t.focus_ring = kFocusFill;  // white ring, as in the designs
    t.scrim = Color::hex(0x05060ACC);
    t.placeholder = kPlaceholder;
    t.radius = kRadius;
    t.ring_width = 4;
    t.ring_gap = 6;
    t.text_size = kBody;
    t.small_size = kCaption;
    t.title_size = kTitle;
    return t;
}

}  // namespace csweb::ui::theme
