// SPDX-License-Identifier: GPL-3.0-or-later
// Built only for CSWEB_LEGACY on webOS. Inspect the kit's retained handle; never initialize again.
#include "wnk/log.hpp"

#include <dlfcn.h>

namespace csweb::app {
void log_legacy_curl_diagnostics() {
    constexpr const char *cat = "legacy-curl";
    void *lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (!lib) {
        const char *error = dlerror();
        WNK_LOGW(cat, "no retained libcurl.so.4: %s", error ? error : "not loaded");
        return;
    }
    void *init = dlsym(lib, "curl_global_init");
    Dl_info info{};
    WNK_LOGI(cat, "loaded library: %s", init && dladdr(init, &info) && info.dli_fname
                                              ? info.dli_fname : "unknown");
    using GetText = const char *(*)();
    auto version = reinterpret_cast<GetText>(dlsym(lib, "curl_version"));
    if (version) WNK_LOGI(cat, "version: %s", version());
    auto diagnostic = reinterpret_cast<GetText>(dlsym(lib, "curl_csweb_legacy_diagnostic"));
    if (diagnostic) WNK_LOGI(cat, "initialization: %s", diagnostic());
    else WNK_LOGW(cat, "initialization diagnostics unavailable in loaded library");
    dlclose(lib);  // balance only our NOLOAD reference; the kit retains its original handle
}
}  // namespace csweb::app
