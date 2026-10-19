// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream's provider test (TestingUtils.runSingleProviderTest, Settings › Providers › Test
// extensions), run through the runtime service with the plugin's own code:
//   1. home page: the first section, when the provider has one (a failure only warns);
//   2. search: up to three queries (the first word of three random home page items, then "over",
//      "iron", "guy") until one returns results; none: fail;
//   3. load and links: the first three results in turn; a result passes when load gives something to
//      play (the first episode's data or the movie's dataUrl) and loadLinks returns at least one link
//      (CustomMedia skips links). The provider passes when one result does.
// Each step is a request; the log lines are the ones CloudStream's Logger collects (no URLs, which
// can carry tokens).
#pragma once

#include "runtime/runtime_client.hpp"
#include "runtime/runtime_types.hpp"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace csweb::runtime {

class ProviderTest {
 public:
    using Request = std::function<RuntimeClient::RequestId(nlohmann::json, RuntimeClient::EventHandler,
                                                           RuntimeClient::DoneHandler)>;
    using Cancel = std::function<void(RuntimeClient::RequestId)>;
    enum class State { Waiting, Running, Passed, Failed };
    struct Line {
        char level;  // 'I', 'W', 'E'
        std::string text;
    };

    ProviderTest(Provider provider, Request request, Cancel cancel, std::function<void()> on_update, uint32_t seed);
    ~ProviderTest();
    ProviderTest(const ProviderTest &) = delete;
    ProviderTest &operator=(const ProviderTest &) = delete;

    void start();
    void stop();  // cancels the open request; the test fails as cancelled
    State state() const { return state_; }
    const Provider &provider() const { return provider_; }
    const std::vector<Line> &log() const { return log_; }
    // "Homepage", "Search", "Load", "Links" while running.
    const std::string &step() const { return step_; }

 private:
    void homepage();
    void search(size_t query);
    void load(size_t result);
    void links(size_t result, const std::string &data);
    void finish(bool passed, const std::string &why);
    void add(char level, std::string text);
    RuntimeClient::DoneHandler guard(std::function<void(bool, const std::string &)> fn);
    void send(nlohmann::json req, RuntimeClient::EventHandler on_event, RuntimeClient::DoneHandler on_done);

    Provider provider_;
    Request request_;
    Cancel cancel_;
    std::function<void()> on_update_;
    std::mt19937 rng_;
    State state_ = State::Waiting;
    std::string step_;
    std::vector<Line> log_;
    std::vector<std::string> queries_;
    std::vector<SearchResult> home_, results_;
    RuntimeClient::RequestId open_ = 0;
    uint64_t answers_ = 0;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

const char *test_state_name(ProviderTest::State s);

}  // namespace csweb::runtime
