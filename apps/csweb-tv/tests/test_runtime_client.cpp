// SPDX-License-Identifier: GPL-3.0-or-later
// runtime/RuntimeClient against tests/fixtures/fake_runtime.py: routing by id, errors, dialogs,
// queueing before ready, and restart after the process exits.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "runtime/runtime_client.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

using namespace csweb::runtime;

namespace {

// The "main thread" of the test: tasks posted from the reader thread run when pump() is called.
struct Loop {
    std::mutex mu;
    std::condition_variable cv;
    std::deque<std::function<void()>> q;
    void post(std::function<void()> f) {
        std::lock_guard<std::mutex> l(mu);
        q.push_back(std::move(f));
        cv.notify_all();
    }
    bool pump_until(const std::function<bool()> &done, int ms = 10000) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (!done()) {
            std::function<void()> f;
            {
                std::unique_lock<std::mutex> l(mu);
                if (!cv.wait_until(l, end, [&] { return !q.empty(); })) return false;
                f = std::move(q.front());
                q.pop_front();
            }
            f();
        }
        return true;
    }
};

RuntimeClient::SpecFactory fake_spec() {
    return [](csweb::core::ChildSpec *s, std::string *) {
        s->path = "/usr/bin/env";
        s->args = {"python3", CSWEB_FIXTURES "/fake_runtime.py"};
        s->env = {"PATH=/usr/bin:/bin"};
        return true;
    };
}

}  // namespace

TEST("search_routed_by_id_and_queued_before_ready") {
    Loop loop;
    RuntimeClient c(fake_spec(), [&](std::function<void()> f) { loop.post(std::move(f)); });
    c.start();
    std::vector<std::string> names;
    bool done = false, ok = false;
    c.request({{"op", "search"}, {"query", "x"}}, [&](const nlohmann::json &e) { names.push_back(e.value("name", "")); },
              [&](bool o, const std::string &) { done = true; ok = o; });
    REQUIRE(loop.pump_until([&] { return done; }));
    CHECK(ok);
    CHECK_EQ(names.size(), size_t(2));
    CHECK(c.state() == RuntimeClient::State::Ready);
    CHECK_EQ(c.open_requests(), size_t(0));
}

TEST("error_message_reaches_done") {
    Loop loop;
    RuntimeClient c(fake_spec(), [&](std::function<void()> f) { loop.post(std::move(f)); });
    c.start();
    std::string error;
    bool done = false;
    c.request({{"op", "nope"}}, {}, [&](bool, const std::string &e) { done = true; error = e; });
    REQUIRE(loop.pump_until([&] { return done; }));
    CHECK_EQ(error, std::string("unknown op nope"));
}

TEST("dialog_answer") {
    Loop loop;
    RuntimeClient c(fake_spec(), [&](std::function<void()> f) { loop.post(std::move(f)); });
    int dialog = 0;
    c.on_dialog = [&](const nlohmann::json &e) { dialog = e.value("dialog", 0); };
    c.start();
    bool done = false, ok = false;
    c.request({{"op", "dialog"}}, {}, [&](bool o, const std::string &) { done = true; ok = o; });
    REQUIRE(loop.pump_until([&] { return dialog != 0; }));
    CHECK_EQ(dialog, 7);
    c.answer_dialog(7, {{"button", "positive"}});
    REQUIRE(loop.pump_until([&] { return done; }));
    CHECK(ok);
}

TEST("restart_after_exit") {
    Loop loop;
    RuntimeClient c(fake_spec(), [&](std::function<void()> f) { loop.post(std::move(f)); });
    int readies = 0;
    c.on_ready = [&] { readies++; };
    c.start();
    bool done = false;
    std::string error;
    c.request({{"op", "crash"}}, {}, [&](bool, const std::string &e) { done = true; error = e; });
    REQUIRE(loop.pump_until([&] { return done; }));
    CHECK_EQ(error, std::string("plugin runtime stopped"));
    REQUIRE(loop.pump_until([&] { return readies == 2; }));
    bool again = false;
    c.request({{"op", "search"}}, {}, [&](bool o, const std::string &) { again = o; });
    REQUIRE(loop.pump_until([&] { return again; }));
}

TEST("spec_failure_marks_failed") {
    Loop loop;
    RuntimeClient c([](csweb::core::ChildSpec *, std::string *e) { *e = "no runtime"; return false; },
                    [&](std::function<void()> f) { loop.post(std::move(f)); });
    c.start();
    CHECK(c.state() == RuntimeClient::State::Failed);
    std::string error;
    c.request({{"op", "search"}}, {}, [&](bool, const std::string &e) { error = e; });
    CHECK_EQ(error, std::string("no runtime"));
}
