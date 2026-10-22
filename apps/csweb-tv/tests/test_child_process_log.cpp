// SPDX-License-Identifier: GPL-3.0-or-later
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/child_process.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>

namespace {
struct Fixture {
    std::filesystem::path root;
    std::string log;
    Fixture() {
        char pattern[] = "/tmp/csweb-log-XXXXXX";
        char *dir = ::mkdtemp(pattern);
        if (!dir) throw std::runtime_error("mkdtemp failed");
        root = dir;
        log = (root / "runtime.log").string();
    }
    ~Fixture() { std::filesystem::remove_all(root); }
};
struct RestrictiveUmask {
    mode_t previous = ::umask(0077);
    ~RestrictiveUmask() { ::umask(previous); }
};

void check_log(bool existing) {
    Fixture f;
    if (existing) {
        std::ofstream(f.log) << "previous\n";
        CHECK_EQ(::chmod(f.log.c_str(), 0600), 0);
    }
    RestrictiveUmask mask;
    std::promise<int> exited;
    auto result = exited.get_future();
    {
        csweb::core::ChildProcess process;
        csweb::core::ChildSpec spec;
        spec.path = "/bin/sh";
        spec.args = {"-c", "printf 'runtime output\\n' >&2"};
        spec.stderr_path = f.log;
        std::string error;
        CHECK(process.start(spec, {}, [&](int status) { exited.set_value(status); }, &error));
        CHECK(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        const int status = result.get();
        CHECK(WIFEXITED(status));
        CHECK_EQ(WEXITSTATUS(status), 0);
    }
    struct stat st {};
    CHECK_EQ(::stat(f.log.c_str(), &st), 0);
#ifdef CSWEB_LEGACY
    CHECK_EQ(st.st_mode & 0777, mode_t(0640));
#else
    CHECK_EQ(st.st_mode & 0777, mode_t(0600));
#endif
    std::ifstream in(f.log);
    const std::string contents((std::istreambuf_iterator<char>(in)), {});
    CHECK_EQ(contents, existing ? std::string("previous\nruntime output\n") : std::string("runtime output\n"));
}
}  // namespace

TEST("new_runtime_log_permissions_ignore_restrictive_umask") { check_log(false); }
TEST("existing_private_runtime_log_is_upgraded_without_truncation") { check_log(true); }

#ifdef CSWEB_LEGACY
TEST("unreadable_log_setup_reports_failure") {
    Fixture f;
    csweb::core::ChildProcess process;
    csweb::core::ChildSpec spec;
    spec.path = "/bin/sh";
    spec.stderr_path = (f.root / "missing" / "runtime.log").string();
    std::string error;
    CHECK(!process.start(spec, {}, {}, &error));
    CHECK(error.find("runtime log permissions:") == 0);
    CHECK(!process.running());
}
#endif
