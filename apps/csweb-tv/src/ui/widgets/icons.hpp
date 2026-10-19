// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bundled SVG icons (assets/icons, white and "-dark" variants), addressed by name. ImageView
// renders them at the view's size.
#pragma once

#include <string>

namespace csweb::ui {

void set_asset_root(const std::string &app_dir);
std::string icon_url(const std::string &name, bool dark = false);

}  // namespace csweb::ui
