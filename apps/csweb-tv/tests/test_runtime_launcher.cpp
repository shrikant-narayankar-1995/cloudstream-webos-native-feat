// SPDX-License-Identifier: GPL-3.0-or-later
// Compiled in both main and legacy modes: inspect real launch plans without executing ART.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "runtime/runtime_launcher.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace {
struct Fixture {
    std::filesystem::path root;
    std::string installed, tmp, data;
    Fixture() {
        char pattern[] = "/tmp/csweb-launch-XXXXXX";
        char *dir = ::mkdtemp(pattern);
        if (!dir) throw std::runtime_error("mkdtemp failed");
        root = dir;
        installed = (root / "installed").string();
        tmp = (root / "state").string();
        data = (root / "data").string();
        std::filesystem::create_directories(installed + "/lib");
        std::filesystem::create_directories(tmp);
        // Both launchers support x86_64 for host validation; no runtime code is executed.
        const std::string loader = installed + "/lib/ld-musl-x86_64.so.1";
        std::ofstream(loader).close();
        ::chmod(loader.c_str(), 0700);
    }
    ~Fixture() { std::filesystem::remove_all(root); }
    void metadata(const std::string &stable) {
        std::ofstream(installed + "/AOT.json") << nlohmann::json{
            {"stable_path", stable}, {"image", "boot/boot.art"},
            {"bootclasspath", {"usr/lib/java/dex/art/second.jar", "usr/lib/java/dex/art/first.jar"}}};
    }
};

std::vector<std::string> boot_env(const csweb::runtime::LaunchPlan &plan) {
    std::vector<std::string> result;
    for (const auto &value : plan.spec.env)
        if (value.compare(0, 14, "BOOTCLASSPATH=") == 0) result.push_back(value);
    return result;
}
}  // namespace

TEST("compiler_bootclasspath_matches_vm_and_preserves_order") {
    Fixture f;
    f.metadata(f.tmp + "/rt");
    const auto plan = csweb::runtime::plan_runtime_launch(f.installed, f.tmp, f.data);
    CHECK(plan.ok);
    const std::string bcp = f.tmp + "/rt/usr/lib/java/dex/art/second.jar:" +
                            f.tmp + "/rt/usr/lib/java/dex/art/first.jar";
    CHECK(std::find(plan.spec.args.begin(), plan.spec.args.end(), "-Xbootclasspath:" + bcp) != plan.spec.args.end());
#ifdef CSWEB_LEGACY
    const auto compiler_env = boot_env(plan);
    CHECK_EQ(compiler_env.size(), size_t(1));
    CHECK_EQ(compiler_env.front(), "BOOTCLASSPATH=" + bcp);
    CHECK(plan.aot_status.find("compiler bootclasspath inherited") != std::string::npos);
#else
    CHECK(boot_env(plan).empty());
    CHECK_EQ(plan.aot_status, std::string("ok"));
#endif
    CHECK_EQ(plan.spec.env.front(), "PATH=" + f.tmp + "/rt/bin");
}

TEST("imageless_launch_has_no_invented_bootclasspath") {
    Fixture f;
    const auto plan = csweb::runtime::plan_runtime_launch(f.installed, f.tmp, f.data);
    CHECK(plan.ok);
    CHECK(boot_env(plan).empty());
    CHECK(plan.aot_status.find("no AOT.json") != std::string::npos);
}

TEST("rejected_image_path_has_no_compiler_bootclasspath_override") {
    Fixture f;
    f.metadata("/tmp/a-different-app/rt");
    const auto plan = csweb::runtime::plan_runtime_launch(f.installed, f.tmp, f.data);
    CHECK(plan.ok);
    CHECK(boot_env(plan).empty());
    CHECK(plan.aot_status.find("AOT built for") != std::string::npos);
}
