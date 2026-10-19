// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app's design tokens (docs/design/screens/): one dark ground, translucent "glass" surfaces, one
// light-blue accent, white for focus and the primary action. Sizes follow the kit's couch-distance
// scale (24 is the floor for content). Every colour and size a screen uses comes from here.
#pragma once

#include "wnk/render/color.hpp"
#include "wnk/ui/ui.hpp"

namespace csweb::ui::theme {

// Profile avatar colours (profiles::kAvatarColours of them).
wnk::render::Color avatar_colour(int index);

using wnk::render::Color;

// Colour roles
inline constexpr Color kBackground = Color::hex(0x0B0D12FF);
inline constexpr Color kGlass = Color::hex(0x161921A0);         // nav bar, panels over artwork
inline constexpr Color kGlassBorder = Color::hex(0xFFFFFF24);
inline constexpr Color kSurface = Color::hex(0xFFFFFF12);       // rows, chips, idle controls
inline constexpr Color kSurfaceRaised = Color::hex(0xFFFFFF1F);
inline constexpr Color kPanel = Color::hex(0x12151CF2);         // side sheets, dialogs
inline constexpr Color kText = Color::hex(0xF4F5F7FF);
inline constexpr Color kTextSecondary = Color::hex(0xD5D9E1FF);
inline constexpr Color kTextMuted = Color::hex(0xA7AEBBFF);
inline constexpr Color kAccent = Color::hex(0x7FB2FFFF);        // progress, links, selected markers
inline constexpr Color kFocusFill = Color::hex(0xF4F5F7FF);     // focused pill, primary action
inline constexpr Color kOnFocus = Color::hex(0x0B0D12FF);       // text on kFocusFill
inline constexpr Color kOk = Color::hex(0x7EE7A6FF);
inline constexpr Color kWarn = Color::hex(0xFFC48AFF);
inline constexpr Color kStar = Color::hex(0xFFC857FF);
inline constexpr Color kPlaceholder = Color::hex(0x232836FF);

// Type scale (logical px at 1920x1080)
inline constexpr float kHero = 88, kDisplay = 64, kTitle = 48, kHeadline = 32, kBody = 28, kLabel = 26,
                       kCaption = 24, kMicro = 20;

// Layout
inline constexpr float kSafeX = 96;        // horizontal safe margin (overscan)
inline constexpr float kSafeTop = 44;
inline constexpr float kPillH = 64;
inline constexpr float kRadius = 16;
inline constexpr float kPosterW = 220, kPosterH = 330;   // 2:3 poster cards
inline constexpr float kStillW = 400, kStillH = 225;     // 16:9 landscape cards

// The kit Theme the shell draws its own widgets (dialogs, toasts, form rows) with.
wnk::ui::Theme make_kit_theme();

}  // namespace csweb::ui::theme
