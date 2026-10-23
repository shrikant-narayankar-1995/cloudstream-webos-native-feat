// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/child_process.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>

#include <fcntl.h>
#ifdef CSWEB_LEGACY
#include <sys/stat.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

namespace csweb::core {

ChildProcess::~ChildProcess() {
    kill();
    close_stdin();
    if (thread_.joinable()) thread_.join();
}

bool ChildProcess::start(const ChildSpec &spec, LineHandler on_line, ExitHandler on_exit, std::string *error,
                         size_t max_line) {
    // A write to a child that has died must fail with EPIPE, not kill the app with SIGPIPE.
    static std::once_flag ignore_sigpipe;
    std::call_once(ignore_sigpipe, [] { std::signal(SIGPIPE, SIG_IGN); });
    if (running_ || thread_.joinable()) {
        if (error) *error = "already started";
        return false;
    }
    int in[2], out[2];
    if (pipe2(in, O_CLOEXEC) != 0) {
        if (error) *error = std::string("pipe: ") + std::strerror(errno);
        return false;
    }
    if (pipe2(out, O_CLOEXEC) != 0) {
        if (error) *error = std::string("pipe: ") + std::strerror(errno);
        ::close(in[0]);
        ::close(in[1]);
        return false;
    }
    const int err_fd = spec.stderr_path.empty()
                           ? ::open("/dev/null", O_WRONLY | O_CLOEXEC)
                           : ::open(spec.stderr_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
#ifdef CSWEB_LEGACY
    // The app and Dev Manager's prisoner account share webOS group 5000. Upgrade existing 0600
    // logs too, and use the open descriptor so umask and pathname races cannot undo the mode.
    if (!spec.stderr_path.empty() && (err_fd < 0 || ::fchmod(err_fd, 0640) != 0)) {
        if (error) *error = std::string("runtime log permissions: ") + std::strerror(errno);
        for (int fd : {in[0], in[1], out[0], out[1], err_fd}) if (fd >= 0) ::close(fd);
        return false;
    }
#endif
    std::vector<char *> argv{const_cast<char *>(spec.path.c_str())};
    for (const auto &a : spec.args) argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);
    std::vector<char *> envp;
    for (const auto &e : spec.env) envp.push_back(const_cast<char *>(e.c_str()));
    envp.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        if (error) *error = std::string("fork: ") + std::strerror(errno);
        for (int fd : {in[0], in[1], out[0], out[1], err_fd}) if (fd >= 0) ::close(fd);
        return false;
    }
    if (pid == 0) {  // async-signal-safe calls only until execve
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        if (err_fd >= 0) dup2(err_fd, STDERR_FILENO);
        if (!spec.working_dir.empty() && chdir(spec.working_dir.c_str()) != 0) _exit(126);
        execve(spec.path.c_str(), argv.data(), envp.data());
        _exit(127);
    }
    ::close(in[0]);
    ::close(out[1]);
    if (err_fd >= 0) ::close(err_fd);
    pid_ = pid;
    in_fd_ = in[1];
    out_fd_ = out[0];
    running_ = true;
    thread_ = std::thread(&ChildProcess::reader, this, std::move(on_line), std::move(on_exit), max_line);
    return true;
}

void ChildProcess::reader(LineHandler on_line, ExitHandler on_exit, size_t max_line) {
    std::string pending;
    bool dropping = false;
    char buf[65536];
    for (;;) {
        const ssize_t n = ::read(out_fd_, buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            const char c = buf[i];
            if (c == '\n') {
                if (dropping) {
                    if (on_line) on_line("<line too long>");
                } else if (on_line) {
                    on_line(std::move(pending));
                }
                pending.clear();
                dropping = false;
            } else if (!dropping) {
                pending.push_back(c);
                if (pending.size() > max_line) {
                    pending.clear();
                    dropping = true;
                }
            }
        }
    }
    ::close(out_fd_);
    out_fd_ = -1;
    int status = 0;
    while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
    }
    running_ = false;
    if (on_exit) on_exit(status);
}

bool ChildProcess::write_line(const std::string &line) {
    std::lock_guard<std::mutex> lock(write_mu_);
    if (in_fd_ < 0 || !running_) return false;
    std::string data = line;
    data.push_back('\n');
    size_t off = 0;
    while (off < data.size()) {
        const ssize_t n = ::write(in_fd_, data.data() + off, data.size() - off);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        off += size_t(n);
    }
    return true;
}

void ChildProcess::close_stdin() {
    std::lock_guard<std::mutex> lock(write_mu_);
    if (in_fd_ >= 0) {
        ::close(in_fd_);
        in_fd_ = -1;
    }
}

void ChildProcess::kill() {
    if (running_ && pid_ > 0) ::kill(pid_, SIGKILL);
}

}  // namespace csweb::core
