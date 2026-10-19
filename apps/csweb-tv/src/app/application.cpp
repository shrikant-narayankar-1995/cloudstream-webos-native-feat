// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/application.hpp"

#include "runtime/runtime_launcher.hpp"
#include "ui/screens/home_screen.hpp"
#include "ui/screens/player_screen.hpp"
#include "ui/widgets/icons.hpp"

#include "wnk/log.hpp"
#include "wnk/media/ndl_backend.hpp"
#include "wnk/platform/process.hpp"

#include <cstdlib>
#include <sys/stat.h>

#ifndef CSWEB_HOST_RUNTIME_DIR
#define CSWEB_HOST_RUNTIME_DIR ""
#endif

namespace csweb::app {

namespace {
constexpr const char *kCat = "csweb";

void make_dir(const std::string &path) { ::mkdir(path.c_str(), 0700); }

std::string runtime_dir(const std::string &app_dir) {
    if (const char *env = std::getenv("CSWEB_RUNTIME_DIR"); env && *env) return env;
    if (wnk::platform::kIsWebOS) return app_dir + "/runtime-aarch64";
    return CSWEB_HOST_RUNTIME_DIR;
}
}  // namespace

Application::Application() = default;

Application::~Application() {
    // on_stop has taken the screens down; stop the runtime so no answer arrives for a dead manager.
    if (runtime_) runtime_->stop();
}

void Application::on_start(wnk::app::Context &ctx) {
    const std::string app_dir = ctx.launch().app_dir;
    std::string tmp = ctx.runtime_dir();
    if (tmp.empty()) tmp = "/tmp/org.csweb.tv";
    make_dir(tmp);
    const std::string plugin_dir = tmp + "/plugins", data_dir = tmp + "/plugin-data";
    make_dir(plugin_dir);
    make_dir(data_dir);
    ui::set_asset_root(app_dir);
    services_.ctx = &ctx;

    settings_ = std::make_unique<settings::SettingsStore>(ctx.store(), ctx.pool());
    settings_->load();
    services_.settings = settings_.get();

    if (auto transport = ctx.http()) {
        http_ = std::make_unique<wnk::net::HttpClient>(transport, ctx.pool(), ctx.dispatcher());
        services_.http = http_.get();
    }

    const std::string rt_dir = runtime_dir(app_dir);
    WNK_LOGI(kCat, "csweb: app %s, runtime %s, tmp %s", app_dir.c_str(), rt_dir.c_str(), tmp.c_str());
    auto &dispatcher = ctx.dispatcher();
    runtime_ = std::make_unique<runtime::RuntimeClient>(
        [rt_dir, tmp, data_dir](core::ChildSpec *spec, std::string *error) {
            runtime::LaunchPlan plan = runtime::plan_runtime_launch(rt_dir, tmp, data_dir);
            if (!plan.ok) {
                *error = plan.error;
                return false;
            }
            WNK_LOGI(kCat, "runtime: launching (AOT: %s)", plan.aot_status.c_str());
            *spec = std::move(plan.spec);
            return true;
        },
        [&dispatcher](std::function<void()> fn) { dispatcher.post(std::move(fn)); });
    services_.runtime = runtime_.get();

    plugins::PluginManager::Deps deps;
    deps.http = ctx.http();
    deps.pool = &ctx.pool();
    deps.main = &ctx.dispatcher();
    deps.kv = ctx.store();
    deps.runtime = runtime_.get();
    deps.plugin_dir = plugin_dir;
    plugins_ = std::make_unique<plugins::PluginManager>(std::move(deps));
    services_.plugins = plugins_.get();
    plugins_->on_changed = [this] { services_.plugins_changed.emit(); };

    runtime_->on_ready = [this] { plugins_->on_runtime_ready(); };
    runtime_->on_state = [this](runtime::RuntimeClient::State) {
        services_.runtime_changed.emit();
        services_.plugins_changed.emit();  // Home shows a runtime failure
    };
    dialogs_ = std::make_unique<ui::PluginDialogHost>(*runtime_);
    runtime_->on_dialog = [this, &ctx](const nlohmann::json &e) { dialogs_->handle(ctx.ui(), e); };

    if (wnk::platform::kIsWebOS) {
        // A Luna call: on a worker, ready long before the first link plays.
        wnk::async::run_then_post<wnk::media::DeviceMediaCaps>(
            ctx.pool(), ctx.dispatcher(), life_.weak(), {},
            [](const wnk::async::CancellationToken &) { return wnk::media::DeviceMediaCaps::detect(); },
            [this](wnk::media::DeviceMediaCaps caps) {
                WNK_LOGI(kCat, "device media: %s", caps.summary().c_str());
                services_.device = std::move(caps);
            });
    }

    wnk::ui::Ui &ui = ctx.ui();
    nav_ = &ui.root().add<wnk::ui::Navigator>();
    router_ = std::make_unique<Router>(services_, *nav_);
    services_.router = router_.get();
    nav_->push(ui, std::make_unique<ui::HomeScreen>(services_));

    plugins_->start();
    runtime_->start();
}

void Application::on_lifecycle(wnk::app::Context &, const wnk::app::Lifecycle::Transition &t) {
    auto *player = nav_ ? dynamic_cast<ui::PlayerScreen *>(nav_->top()) : nullptr;
    if (t.to == wnk::app::State::Suspended) {
        if (player) player->pause_for_background();
        if (wnk::platform::kIsWebOS) wnk::media::NdlBackend::set_app_foreground(false);
    } else if (t.to == wnk::app::State::Resuming && wnk::platform::kIsWebOS) {
        wnk::media::NdlBackend::set_app_foreground(true);
    } else if (t.from == wnk::app::State::Resuming && t.to == wnk::app::State::Running) {
        if (player) player->return_from_background();
    }
}

void Application::on_stop(wnk::app::Context &ctx) {
    wnk::ui::Ui &ui = ctx.ui();
    while (wnk::ui::Dialog *d = ui.top_dialog()) ui.dismiss_dialog(d);
    // The shell destroys the delegate before the Ui, and every screen refers to the services: take
    // the screens down now (the player stops, requests are cancelled) while the services exist.
    if (nav_) {
        std::unique_ptr<wnk::ui::View> screens = ui.root().take_child(nav_);
        nav_ = nullptr;
        screens.reset();
    }
    if (runtime_) runtime_->stop();
}

}  // namespace csweb::app
