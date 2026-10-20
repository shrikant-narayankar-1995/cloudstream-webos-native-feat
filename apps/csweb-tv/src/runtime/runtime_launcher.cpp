// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/runtime_launcher.hpp"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstring>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

namespace csweb::runtime {

namespace {

bool exists(const std::string &p) { return access(p.c_str(), F_OK) == 0; }

std::string read_first_line(const std::string &path) {
    std::ifstream f(path);
    std::string line;
    if (f) std::getline(f, line);
    return line;
}

struct Aot {
    std::string base;  // the stable symlink, or empty
    std::vector<std::string> options;
    std::string status;
};

// Mirrors apps/runtime-probe prepare_aot: an existing non-symlink at the stable path is never replaced.
Aot prepare_aot(const std::string &installed, const std::string &tmp) {
    std::ifstream f(installed + "/AOT.json");
    if (!f) return {{}, {}, "no AOT.json"};
    const auto aot = nlohmann::json::parse(f, nullptr, false);
    const std::string link = tmp + "/rt";
    if (!aot.is_object() || !aot.contains("bootclasspath") || !aot["bootclasspath"].is_array()) return {{}, {}, "bad AOT.json"};
    if (aot.value("stable_path", "") != link) return {{}, {}, "AOT built for " + aot.value("stable_path", "?")};
    struct stat st {};
    if (lstat(link.c_str(), &st) == 0) {
        if (!S_ISLNK(st.st_mode)) return {{}, {}, link + " exists and is not a symlink"};
        if (unlink(link.c_str()) != 0) return {{}, {}, "unlink " + link + ": " + std::strerror(errno)};
    }
    if (symlink(installed.c_str(), link.c_str()) != 0) return {{}, {}, "symlink " + link + ": " + std::strerror(errno)};
    std::string bcp;
    for (const auto &j : aot["bootclasspath"]) {
        if (!j.is_string() || j.get<std::string>().find("..") != std::string::npos) return {{}, {}, "bad bootclasspath entry"};
        bcp += (bcp.empty() ? "" : ":") + link + "/" + j.get<std::string>();
    }
    return {link, {"-Ximage:" + link + "/" + aot.value("image", "boot/boot.art"), "-Xbootclasspath:" + bcp}, "ok"};
}

}  // namespace

LaunchPlan plan_runtime_launch(const std::string &runtime_dir, const std::string &tmp_dir, const std::string &data_dir,
                               int heap_mb) {
    LaunchPlan plan;
    std::string loader;
    for (const char *name : {"ld-musl-aarch64.so.1", "ld-musl-x86_64.so.1"}) {
        if (access((runtime_dir + "/lib/" + name).c_str(), X_OK) == 0) loader = name;
    }
    if (loader.empty()) {
        plan.error = "no plugin runtime in " + runtime_dir;
        return plan;
    }
    if (tmp_dir.empty()) {
        plan.error = "no writable runtime directory";
        return plan;
    }
    for (const std::string &d : {tmp_dir + "/art", tmp_dir + "/home", data_dir}) mkdir(d.c_str(), 0700);

    const Aot aot = prepare_aot(runtime_dir, tmp_dir);
    plan.aot_status = aot.status;
    const std::string rt = aot.base.empty() ? runtime_dir : aot.base;
    const std::string icu = read_first_line(runtime_dir + "/ICU_VERSION");
    const std::string libs = rt + "/usr/lib:" + rt + "/usr/lib/art:" + rt + "/usr/lib/java/dex/art/natives:" + rt + "/lib";

    auto &s = plan.spec;
    s.path = rt + "/lib/" + loader;
    s.args = {"--library-path", libs};
    // low4g.so (aarch64 runtimes): replaces ART's low-4GB allocator, which never finishes on some TVs
    if (access((runtime_dir + "/lib/low4g.so").c_str(), R_OK) == 0) {
        s.args.push_back("--preload");
        s.args.push_back(rt + "/lib/low4g.so");
    }
    s.args.insert(s.args.end(), {rt + "/usr/bin/dalvikvm", "-Xmx" + std::to_string(heap_mb) + "m",
              "-Xnoimage-dex2oat", "-Xcompiler-option", "--compiler-filter=verify"});
    s.args.insert(s.args.end(), aot.options.begin(), aot.options.end());
    for (std::string a : {"-Djavax.net.ssl.trustStore=" + rt + "/trust/cacerts",
                          "-Djava.library.path=" + rt + "/usr/lib/java/dex/art/natives", std::string("-cp"),
                          rt + "/csweb/csweb-host.dex:" + rt + "/csweb/cloudstream.jar", std::string("org.csweb.loader.Main"),
                          std::string("--data"), data_dir, std::string("--serve"), std::string("1")}) {
        s.args.push_back(std::move(a));
    }
    s.env = {"PATH=" + rt + "/bin", "ANDROID_DATA=" + tmp_dir + "/art", "HOME=" + tmp_dir + "/home"};
    if (!icu.empty() && exists(runtime_dir + "/usr/share/icu/" + icu)) s.env.push_back("ICU_DATA=" + rt + "/usr/share/icu/" + icu);
    s.stderr_path = tmp_dir + "/runtime.log";
    plan.ok = true;
    return plan;
}

}  // namespace csweb::runtime
