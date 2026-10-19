// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/provider_test.hpp"

#include <algorithm>

namespace csweb::runtime {

namespace {
constexpr size_t kQueries = 3;
constexpr size_t kResults = 3;

std::string first_word(const std::string &name) {
    const size_t sp = name.find(' ');
    return sp == std::string::npos ? name : name.substr(0, sp);
}
}  // namespace

const char *test_state_name(ProviderTest::State s) {
    switch (s) {
        case ProviderTest::State::Waiting: return "Waiting";
        case ProviderTest::State::Running: return "Running";
        case ProviderTest::State::Passed: return "Passed";
        case ProviderTest::State::Failed: return "Failed";
    }
    return "?";
}

ProviderTest::ProviderTest(Provider provider, Request request, Cancel cancel, std::function<void()> on_update, uint32_t seed)
    : provider_(std::move(provider)),
      request_(std::move(request)),
      cancel_(std::move(cancel)),
      on_update_(std::move(on_update)),
      rng_(seed) {}

ProviderTest::~ProviderTest() {
    *alive_ = false;
    if (open_ != 0 && cancel_) cancel_(open_);
}

RuntimeClient::DoneHandler ProviderTest::guard(std::function<void(bool, const std::string &)> fn) {
    std::weak_ptr<bool> alive = alive_;
    return [this, alive, fn = std::move(fn)](bool ok, const std::string &error) {
        auto a = alive.lock();
        if (!a || !*a || state_ != State::Running) return;
        open_ = 0;
        ++answers_;
        fn(ok, error);
    };
}

void ProviderTest::send(nlohmann::json req, RuntimeClient::EventHandler on_event, RuntimeClient::DoneHandler on_done) {
    // A request may be answered before request() returns (the runtime is not running): its id is
    // then no longer open.
    const uint64_t before = answers_;
    const RuntimeClient::RequestId id = request_(std::move(req), std::move(on_event), std::move(on_done));
    if (answers_ == before && state_ == State::Running) open_ = id;
}

void ProviderTest::add(char level, std::string text) {
    log_.push_back({level, std::move(text)});
    if (on_update_) on_update_();
}

void ProviderTest::start() {
    if (state_ == State::Running) return;
    state_ = State::Running;
    log_.clear();
    home_.clear();
    results_.clear();
    add('I', "Trying " + provider_.name);
    homepage();
}

void ProviderTest::stop() {
    if (state_ != State::Running) return;
    const RuntimeClient::RequestId id = open_;
    open_ = 0;
    finish(false, "Testing of " + provider_.name + " was cancelled");
    if (id != 0 && cancel_) cancel_(id);
}

void ProviderTest::finish(bool passed, const std::string &why) {
    state_ = passed ? State::Passed : State::Failed;
    step_.clear();
    add(passed ? 'I' : 'E', why);
}

void ProviderTest::homepage() {
    if (!provider_.has_main_page || provider_.main_page.empty()) {
        search(0);
        return;
    }
    step_ = "Homepage";
    auto items = std::make_shared<std::vector<SearchResult>>();
    send({{"op", "mainpage"}, {"provider", provider_.name}, {"section", 0}, {"page", 1}, {"limit", 60}},
                     [items](const nlohmann::json &e) {
                         if (e.value("event", "") == "result") items->push_back(parse_search_result(e));
                     },
                     guard([this, items](bool ok, const std::string &error) {
                         if (!ok) {
                             add('W', "Exception thrown when loading homepage: \"" + error + "\"");
                         } else if (items->empty()) {
                             add('W', "Provider " + provider_.name + " does not have any items in a homepage row!");
                         }
                         home_ = std::move(*items);
                         search(0);
                     }));
}

void ProviderTest::search(size_t query) {
    if (query == 0) {
        // Three random home page items' first words, then the generic queries.
        std::vector<SearchResult> shuffled = home_;
        std::shuffle(shuffled.begin(), shuffled.end(), rng_);
        queries_.clear();
        for (size_t i = 0; i < shuffled.size() && queries_.size() < kQueries; i++) {
            const std::string w = first_word(shuffled[i].name);
            if (!w.empty()) queries_.push_back(w);
        }
        for (const char *q : {"over", "iron", "guy"}) {
            if (queries_.size() < kQueries) queries_.push_back(q);
        }
    }
    if (query >= queries_.size()) {
        finish(false, "Api " + provider_.name + " did not return any search responses");
        return;
    }
    step_ = "Search";
    add('I', "Searching for: " + queries_[query]);
    auto items = std::make_shared<std::vector<SearchResult>>();
    send({{"op", "search"}, {"provider", provider_.name}, {"query", queries_[query]}, {"limit", 60}},
                     [items](const nlohmann::json &e) {
                         if (e.value("event", "") == "result") items->push_back(parse_search_result(e));
                     },
                     guard([this, items, query](bool ok, const std::string &error) {
                         if (!ok && !error.empty()) add('W', "Search failed: " + error);
                         if (items->empty()) {
                             search(query + 1);
                             return;
                         }
                         results_ = std::move(*items);
                         load(0);
                     }));
}

void ProviderTest::load(size_t result) {
    if (result >= results_.size() || result >= kResults) {
        finish(false, "Link loading failed");
        return;
    }
    const SearchResult &r = results_[result];
    step_ = "Load";
    add('I', "Testing search result: " + r.name);
    if (!r.api_name.empty() && r.api_name != provider_.name)
        add('W', "Wrong apiName on SearchResponse: " + provider_.name + " != " + r.api_name);
    auto details = std::make_shared<Details>();
    auto got = std::make_shared<bool>(false);
    auto first_episode = std::make_shared<std::string>();
    auto episodes = std::make_shared<size_t>(0);
    send({{"op", "details"}, {"provider", provider_.name}, {"url", r.url}, {"limit", 1}},
                     [details, got, first_episode, episodes](const nlohmann::json &e) {
                         const std::string ev = e.value("event", "");
                         if (ev == "details") {
                             *details = parse_details(e);
                             *got = true;
                         } else if (ev == "episode") {
                             if (++*episodes == 1) *first_episode = parse_episode(e).data;
                         }
                     },
                     guard([this, result, details, got, first_episode, episodes](bool, const std::string &error) {
                         if (!*got) {
                             add('E', "Returned null loadResponse on " + provider_.name +
                                          (error.empty() ? std::string() : ": " + error));
                             load(result + 1);
                             return;
                         }
                         const Details &d = *details;
                         if (!d.api_name.empty() && d.api_name != provider_.name)
                             add('W', "Wrong apiName on LoadResponse: " + provider_.name + " != " + d.api_name);
                         if (!d.type.empty() &&
                             std::find(provider_.types.begin(), provider_.types.end(), d.type) == provider_.types.end())
                             add('W', "Api " + provider_.name + " on load does not contain any of the supportedTypes: " + d.type);
                         std::string data;
                         if (*episodes > 0) {
                             data = *first_episode;
                         } else if (!d.data_url.empty()) {
                             data = d.data_url;
                         } else {
                             add('E', "Api " + provider_.name + " got no episodes on " + d.name);
                             load(result + 1);
                             return;
                         }
                         if (d.type == "CustomMedia") {
                             add('I', "Skipping link loading test");
                             finish(true, "Success " + provider_.name);
                             return;
                         }
                         links(result, data);
                     }));
}

void ProviderTest::links(size_t result, const std::string &data) {
    step_ = "Links";
    auto count = std::make_shared<size_t>(0);
    auto bad = std::make_shared<size_t>(0);
    send({{"op", "links"}, {"provider", provider_.name}, {"url", data}},
                     [count, bad](const nlohmann::json &e) {
                         if (e.value("event", "") != "link") return;
                         const Link l = parse_link(e);
                         if (l.url.size() > 4) ++*count;
                         else ++*bad;
                     },
                     guard([this, result, count, bad](bool ok, const std::string &error) {
                         if (*bad > 0) add('E', "Api " + provider_.name + " returns " + std::to_string(*bad) + " link(s) with an invalid url");
                         if (ok && *count > 0) {
                             add('I', "Links loaded: " + std::to_string(*count));
                             finish(true, "Success " + provider_.name);
                             return;
                         }
                         add('E', "Failed link loading on " + provider_.name +
                                      (error.empty() ? " (" + std::to_string(*count) + " links)" : ": " + error));
                         load(result + 1);
                     }));
}

}  // namespace csweb::runtime
