// SPDX-License-Identifier: GPL-3.0-or-later
//
// CSWeb TV: a native webOS app that runs unchanged CloudStream plugins on the TV (apps/csweb-tv/README.md).
#include "app/application.hpp"
#include "ui/theme/theme.hpp"

#include <cstdlib>

int main(int argc, char **argv) {
    wnk::app::AppConfig config;
    config.fallback_app_id = "org.csweb.tv";
    config.title = "CSWeb TV";
    config.worker_threads = 4;
    config.http = true;
    config.storage = true;
    config.theme = csweb::ui::theme::make_kit_theme();
    config.script_env_prefix = "WNK_CSWEB";
    // Scripted host runs (WNK_CSWEB_KEYS) step once per drawn frame: drawing every frame keeps them fast.
    if (std::getenv("WNK_CSWEB_KEYS")) config.redraw_on_change = false;
    return wnk::app::run(argc, argv, config, [] { return std::make_unique<csweb::app::Application>(); });
}
