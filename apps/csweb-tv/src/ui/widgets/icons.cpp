// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widgets/icons.hpp"

namespace csweb::ui {

namespace {
std::string &root() {
    static std::string r;
    return r;
}
}  // namespace

void set_asset_root(const std::string &app_dir) { root() = app_dir; }

std::string icon_url(const std::string &name, bool dark) {
    if (name.empty()) return {};
    return "file://" + root() + "/icons/" + name + (dark && name != "star" ? "-dark" : "") + ".svg";
}

std::string asset_url(const std::string &path) { return "file://" + root() + "/" + path; }

}  // namespace csweb::ui
