// SPDX-License-Identifier: GPL-3.0-or-later
//
// A child process with a line protocol on stdin/stdout: the plugin runtime (ART) runs this way, so a
// plugin that crashes or hangs takes down only its process, never the app. The kit has no process
// API, so this is plain fork/execve with pipes.
//
// Threading: start() and write_line() are safe from any thread. Output lines and the exit are
// delivered on the reader thread this class owns; callers post them to the main thread themselves.
// The destructor kills the child (if still running) and joins the reader.
#pragma once

#include <sys/types.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace csweb::core {

struct ChildSpec {
    std::string path;               // executable
    std::vector<std::string> args;  // argv[1..]
    std::vector<std::string> env;   // the complete environment ("K=V"); nothing is inherited
    std::string stderr_path;        // appended to; empty: /dev/null
    std::string working_dir;        // empty: unchanged
};

class ChildProcess {
 public:
    using LineHandler = std::function<void(std::string line)>;
    using ExitHandler = std::function<void(int wait_status)>;  // after the last line

    ChildProcess() = default;
    ~ChildProcess();
    ChildProcess(const ChildProcess &) = delete;
    ChildProcess &operator=(const ChildProcess &) = delete;

    // Lines longer than `max_line` bytes are dropped (one "<line too long>" marker is delivered instead).
    bool start(const ChildSpec &spec, LineHandler on_line, ExitHandler on_exit, std::string *error,
               size_t max_line = 8u << 20);
    // Writes `line` plus '\n'. False once the child's stdin is closed or the child is gone.
    bool write_line(const std::string &line);
    void close_stdin();
    // SIGKILL; the exit is still reported through the ExitHandler.
    void kill();
    bool running() const { return running_; }
    pid_t pid() const { return pid_; }

 private:
    void reader(LineHandler on_line, ExitHandler on_exit, size_t max_line);
    pid_t pid_ = -1;
    int in_fd_ = -1;   // child's stdin (we write)
    int out_fd_ = -1;  // child's stdout (we read)
    std::mutex write_mu_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}  // namespace csweb::core
