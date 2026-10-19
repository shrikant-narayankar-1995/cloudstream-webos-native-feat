// SPDX-License-Identifier: GPL-3.0-or-later
//
// How the plugin runtime (ART, runtime/make_dist.py) is started: through its bundled musl loader,
// with a cleared environment, writable state on tmpfs, and — when the package carries ahead-of-time
// files (runtime/make-aot.sh) built for this app's stable path — a boot image and precompiled class
// path reached through the symlink <tmp>/rt. Without them ART boots imageless (slower, same result).
// Same launch as the measured probe runs (docs/runtime/aot.md, docs/runtime/service-protocol.md).
#pragma once

#include "core/child_process.hpp"

#include <string>

namespace csweb::runtime {

struct LaunchPlan {
    bool ok = false;
    std::string error;       // why the runtime cannot start (no runtime bundled, ...)
    core::ChildSpec spec;
    std::string aot_status;  // "ok" or why the AOT files were not used
};

// `runtime_dir`: the installed runtime (…/runtime-aarch64). `tmp_dir`: the app's tmpfs directory
// (ctx.runtime_dir()). `data_dir`: plugin data (SharedPreferences of plugins). The service is started
// with --serve; stderr goes to <tmp_dir>/runtime.log.
LaunchPlan plan_runtime_launch(const std::string &runtime_dir, const std::string &tmp_dir,
                               const std::string &data_dir, int heap_mb = 256);

}  // namespace csweb::runtime
