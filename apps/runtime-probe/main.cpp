// SPDX-License-Identifier: GPL-3.0-or-later
// CloudStream webOS feasibility probe (NOT a CloudStream-compatible app, and runs no plugin code).
// Answers the platform questions an on-device Android runtime depends on: may the jailed app start a
// second process from its own directory and from tmpfs, may that process run a 64-bit binary, map
// code executable (AOT files, JIT), get entropy, threads and network? Results go to events.log
// (tools/logs.sh) and to the screen.
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "child_probe.hpp"
#include "wnk/app/app.hpp"
#include "wnk/async/async.hpp"
#include "wnk/log.hpp"
#include "wnk/ui/forms.hpp"
#include "wnk/ui/navigation.hpp"
#include "wnk/ui/widgets.hpp"

namespace {

constexpr const char *kCat = "csprobe";
constexpr const char *kChildFlag = "--child-probe";
constexpr int kChildTimeoutMs = 30000;

struct RunResult {
    std::string label;
    std::string status;  // "exit N", "signal N", "exec failed: ...", "timeout"
    std::vector<std::string> lines;
};

// fork + execve with a fixed environment; stdout and stderr captured; killed after the timeout.
RunResult run(const std::string &label, const std::string &path, const std::vector<std::string> &args,
              const std::vector<std::string> &env) {
    RunResult r{label, {}, {}};
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        r.status = std::string("pipe failed: ") + std::strerror(errno);
        return r;
    }
    std::vector<char *> argv{const_cast<char *>(path.c_str())};
    for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);
    std::vector<char *> envp;
    for (const auto &e : env) envp.push_back(const_cast<char *>(e.c_str()));
    envp.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        r.status = std::string("fork failed: ") + std::strerror(errno);
        close(fds[0]);
        close(fds[1]);
        return r;
    }
    if (pid == 0) {  // only async-signal-safe calls until execve
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        execve(path.c_str(), argv.data(), envp.data());
        const int e = errno;
        static const char msg[] = "execve failed errno=";
        (void)!write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        char num[4] = {static_cast<char>('0' + (e / 100) % 10), static_cast<char>('0' + (e / 10) % 10),
                       static_cast<char>('0' + e % 10), '\n'};
        (void)!write(STDOUT_FILENO, num, sizeof(num));
        _exit(127);
    }
    close(fds[1]);
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kChildTimeoutMs);
    bool timed_out = false;
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            timed_out = true;
            break;
        }
        struct pollfd p {fds[0], POLLIN, 0};
        if (poll(&p, 1, static_cast<int>(left.count())) <= 0) continue;
        char buf[4096];
        const ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (out.size() < 64 * 1024) out.append(buf, static_cast<size_t>(n));
    }
    close(fds[0]);
    if (timed_out) kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    if (timed_out) {
        r.status = "timeout";
    } else if (WIFEXITED(status)) {
        r.status = "exit " + std::to_string(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        r.status = "signal " + std::to_string(WTERMSIG(status));
    }
    std::istringstream in(out);
    for (std::string line; std::getline(in, line);) r.lines.push_back(line);
    return r;
}

bool write_executable(const std::string &path, const void *data, size_t size, std::string &error) {
    unlink(path.c_str());
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0700);
    if (fd < 0) {
        error = std::string("create: ") + std::strerror(errno);
        return false;
    }
    const bool ok = write(fd, data, size) == static_cast<ssize_t>(size);
    if (!ok) error = std::string("write: ") + std::strerror(errno);
    close(fd);
    return ok;
}

bool copy_file(const std::string &from, const std::string &to, std::string &error) {
    int in = open(from.c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        error = std::string("open: ") + std::strerror(errno);
        return false;
    }
    std::string data;
    char buf[65536];
    for (ssize_t n; (n = read(in, buf, sizeof(buf))) > 0;) data.append(buf, static_cast<size_t>(n));
    close(in);
    return write_executable(to, data.data(), data.size(), error);
}

std::vector<RunResult> run_probes(const std::string &exe, const std::string &app_dir, const std::string &tmp) {
    std::vector<RunResult> results;
    const std::vector<std::string> env{"PATH=/usr/bin:/bin", "HOME=" + tmp, "LD_LIBRARY_PATH=" + app_dir + "/lib"};

    // 1. The packaged ARM executable, from the read-only app directory, with network checks.
    results.push_back(run("exec_app_dir", exe, {kChildFlag, app_dir, tmp, "net"}, env));

    // 2. A copy on tmpfs: is a writable filesystem also an executable one (an unpacked runtime, oat files)?
    std::string error;
    const std::string copy = tmp + "/csprobe-copy";
    if (copy_file(exe, copy, error)) {
        results.push_back(run("exec_tmpfs_copy", copy, {kChildFlag, app_dir, tmp, "nonet"}, env));
        unlink(copy.c_str());
    } else {
        results.push_back({"exec_tmpfs_copy", "copy failed: " + error, {}});
    }

    // 3. A 64-bit AArch64 program (an arm64 runtime build is the alternative to an armv7 one).
    const auto elf = csprobe::aarch64_exit42_elf();
    const std::string a64 = tmp + "/csprobe-a64";
    if (write_executable(a64, elf.data(), elf.size(), error)) {
        results.push_back(run("exec_aarch64", a64, {}, env));
        unlink(a64.c_str());
    } else {
        results.push_back({"exec_aarch64", "write failed: " + error, {}});
    }
    return results;
}

class ProbeScreen : public wnk::ui::Screen {
 public:
    ProbeScreen() : Screen("probe") {
        name = "probe";
        auto &col = add<wnk::ui::Column>(24);
        col.set_padding(wnk::ui::Insets::all(60));
        col.add<wnk::ui::Label>("Runtime feasibility probe — not CloudStream-compatible, runs no plugins",
                                wnk::ui::TextRole::Title);
        text_ = &col.add<wnk::ui::TextView>("Running…", wnk::ui::TextRole::Small);
        text_->set_width(wnk::ui::Dim::fill());
        text_->set_height(wnk::ui::Dim::fill());
    }
    void show(const std::string &s) { text_->set_text(s); }

 private:
    wnk::ui::TextView *text_ = nullptr;
};

class App : public wnk::app::Delegate {
 public:
    void on_start(wnk::app::Context &ctx) override {
        auto &ui = ctx.ui();
        nav_ = &ui.root().add<wnk::ui::Navigator>();
        auto screen = std::make_unique<ProbeScreen>();
        screen_ = screen.get();
        nav_->push(ui, std::move(screen));

        const std::string exe = ctx.launch().exe_path;
        const std::string app_dir = ctx.launch().app_dir;
        const std::string tmp = ctx.runtime_dir();
        WNK_LOGI(kCat, "probe start exe=%s app_dir=%s runtime_dir=%s", exe.c_str(), app_dir.c_str(), tmp.c_str());
        if (tmp.empty()) {
            screen_->show("No runtime directory: cannot run the probe.");
            WNK_LOGE(kCat, "no runtime dir");
            return;
        }
        wnk::async::run_then_post<std::vector<RunResult>>(
                ctx.pool(), ctx.dispatcher(), lifetime_.weak(), {},
                [exe, app_dir, tmp](const wnk::async::CancellationToken &) { return run_probes(exe, app_dir, tmp); },
                [this](std::vector<RunResult> results) {
                    std::string text;
                    for (const auto &r : results) {
                        WNK_LOGI(kCat, "run %s status=%s lines=%zu", r.label.c_str(), r.status.c_str(), r.lines.size());
                        text += "[" + r.label + "] " + r.status + "\n";
                        for (const auto &line : r.lines) {
                            WNK_LOGI(kCat, "%s %s", r.label.c_str(), line.c_str());
                            text += "  " + line + "\n";
                        }
                    }
                    WNK_LOGI(kCat, "probe done");
                    screen_->show(text);
                });
    }

 private:
    wnk::async::Lifetime lifetime_;
    wnk::ui::Navigator *nav_ = nullptr;
    ProbeScreen *screen_ = nullptr;
};

}  // namespace

int main(int argc, char **argv) {
    if (argc >= 5 && std::strcmp(argv[1], kChildFlag) == 0) {
        return csprobe::run_child_probe(argv[2], argv[3], std::strcmp(argv[4], "net") == 0);
    }
    wnk::app::AppConfig config;
    config.fallback_app_id = "org.csweb.runtimeprobe";
    config.title = "CS runtime probe";
    config.script_env_prefix = "CSWEB_PROBE";
    return wnk::app::run(argc, argv, config, [] { return std::make_unique<App>(); });
}
