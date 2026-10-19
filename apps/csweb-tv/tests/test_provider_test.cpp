// SPDX-License-Identifier: GPL-3.0-or-later
// runtime/provider_test: CloudStream's runSingleProviderTest steps against a scripted runtime.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "runtime/provider_test.hpp"

#include <deque>
#include <functional>
#include <map>

using namespace csweb::runtime;
using nlohmann::json;

namespace {

// Answers requests later (as the runtime does), in order, from a script keyed by op.
struct FakeRuntime {
    struct Pending {
        json req;
        RuntimeClient::EventHandler on_event;
        RuntimeClient::DoneHandler on_done;
        RuntimeClient::RequestId id;
    };
    std::function<void(const json &req, std::vector<json> *events, bool *ok, std::string *error)> script;
    std::deque<Pending> pending;
    std::vector<json> seen;
    std::vector<RuntimeClient::RequestId> cancelled;
    RuntimeClient::RequestId next = 1;

    ProviderTest::Request request() {
        return [this](json req, RuntimeClient::EventHandler ev, RuntimeClient::DoneHandler done) {
            seen.push_back(req);
            pending.push_back({std::move(req), std::move(ev), std::move(done), next});
            return next++;
        };
    }
    ProviderTest::Cancel cancel() {
        return [this](RuntimeClient::RequestId id) { cancelled.push_back(id); };
    }
    void run() {
        while (!pending.empty()) {
            Pending p = std::move(pending.front());
            pending.pop_front();
            std::vector<json> events;
            bool ok = true;
            std::string error;
            script(p.req, &events, &ok, &error);
            for (const auto &e : events) p.on_event(e);
            p.on_done(ok, error);
        }
    }
};

json result(const std::string &name, const std::string &url) {
    return {{"event", "result"}, {"name", name}, {"url", url}, {"apiName", "Prov"}};
}

Provider provider(bool home) {
    Provider p;
    p.name = "Prov";
    p.types = {"Movie"};
    p.has_main_page = home;
    if (home) p.main_page = {"Popular"};
    return p;
}

std::string ops(const FakeRuntime &rt) {
    std::string s;
    for (const auto &r : rt.seen) s += (s.empty() ? "" : ",") + r.value("op", "");
    return s;
}

}  // namespace

TEST("passes_through_home_search_load_links") {
    FakeRuntime rt;
    rt.script = [](const json &req, std::vector<json> *ev, bool *, std::string *) {
        const std::string op = req["op"];
        if (op == "mainpage") ev->push_back(result("Alpha Movie", "u1"));
        if (op == "search") ev->push_back(result("Alpha Movie", "u1"));
        if (op == "details") ev->push_back({{"event", "details"}, {"name", "Alpha"}, {"type", "Movie"}, {"dataUrl", "d1"}, {"apiName", "Prov"}});
        if (op == "links") ev->push_back({{"event", "link"}, {"url", "https://h/v.mp4"}, {"name", "L"}});
    };
    ProviderTest t(provider(true), rt.request(), rt.cancel(), {}, 1);
    t.start();
    CHECK(t.state() == ProviderTest::State::Running);
    rt.run();
    CHECK(t.state() == ProviderTest::State::Passed);
    CHECK_EQ(ops(rt), std::string("mainpage,search,details,links"));
    CHECK_EQ(rt.seen[1]["query"].get<std::string>(), std::string("Alpha"));  // first word of a home item
    CHECK_EQ(rt.seen[3]["url"].get<std::string>(), std::string("d1"));
    CHECK_EQ(t.log().back().text, std::string("Success Prov"));
}

TEST("generic_queries_until_results") {
    FakeRuntime rt;
    rt.script = [](const json &req, std::vector<json> *ev, bool *, std::string *) {
        const std::string op = req["op"];
        if (op == "search" && req["query"] == "guy") ev->push_back(result("Guy", "u"));
        if (op == "details") {
            ev->push_back({{"event", "details"}, {"name", "Guy"}, {"type", "TvSeries"}});
            ev->push_back({{"event", "episode"}, {"data", "ep1"}});
            ev->push_back({{"event", "episode"}, {"data", "ep2"}});
        }
        if (op == "links") ev->push_back({{"event", "link"}, {"url", "https://h/v.m3u8"}});
    };
    ProviderTest t(provider(false), rt.request(), rt.cancel(), {}, 1);
    t.start();
    rt.run();
    CHECK(t.state() == ProviderTest::State::Passed);
    CHECK_EQ(ops(rt), std::string("search,search,search,details,links"));
    CHECK_EQ(rt.seen.back()["url"].get<std::string>(), std::string("ep1"));
    bool warned = false;  // TvSeries is not among the provider's types
    for (const auto &l : t.log()) warned |= l.level == 'W' && l.text.find("supportedTypes") != std::string::npos;
    CHECK(warned);
}

TEST("fails_without_search_results") {
    FakeRuntime rt;
    rt.script = [](const json &req, std::vector<json> *, bool *ok, std::string *error) {
        if (req["op"] == "mainpage") {
            *ok = false;
            *error = "HTTP 500";
        }
    };
    ProviderTest t(provider(true), rt.request(), rt.cancel(), {}, 1);
    t.start();
    rt.run();
    CHECK(t.state() == ProviderTest::State::Failed);
    CHECK_EQ(ops(rt), std::string("mainpage,search,search,search"));
    CHECK_EQ(t.log()[1].level, 'W');  // the home page only warns
    CHECK_EQ(t.log().back().text, std::string("Api Prov did not return any search responses"));
}

TEST("tries_three_results_then_fails") {
    FakeRuntime rt;
    rt.script = [](const json &req, std::vector<json> *ev, bool *ok, std::string *error) {
        const std::string op = req["op"];
        if (op == "search")
            for (int i = 0; i < 5; i++) ev->push_back(result("R" + std::to_string(i), "u" + std::to_string(i)));
        if (op == "details") {
            if (req["url"] == "u0") {
                *ok = false;
                *error = "boom";
            } else if (req["url"] == "u1") {
                ev->push_back({{"event", "details"}, {"name", "R1"}, {"type", "Movie"}});  // nothing to play
            } else {
                ev->push_back({{"event", "details"}, {"name", "R2"}, {"type", "Movie"}, {"dataUrl", "d"}});
            }
        }
        if (op == "links") ev->push_back({{"event", "link"}, {"url", "x"}});  // invalid url
    };
    ProviderTest t(provider(false), rt.request(), rt.cancel(), {}, 1);
    t.start();
    rt.run();
    CHECK(t.state() == ProviderTest::State::Failed);
    CHECK_EQ(ops(rt), std::string("search,details,details,details,links"));
    CHECK_EQ(t.log().back().text, std::string("Link loading failed"));
}

TEST("stop_cancels_and_ignores_late_answers") {
    FakeRuntime rt;
    rt.script = [](const json &, std::vector<json> *ev, bool *, std::string *) { ev->push_back(result("A", "u")); };
    ProviderTest t(provider(false), rt.request(), rt.cancel(), {}, 1);
    t.start();
    t.stop();
    CHECK(t.state() == ProviderTest::State::Failed);
    CHECK_EQ(rt.cancelled.size(), size_t(1));
    rt.run();  // the cancelled search still answers
    CHECK(t.state() == ProviderTest::State::Failed);
    CHECK_EQ(rt.seen.size(), size_t(1));
}

TEST("synchronous_answers") {
    // A runtime that is not running answers inside request().
    int calls = 0;
    ProviderTest::Request req = [&calls](json, RuntimeClient::EventHandler, RuntimeClient::DoneHandler done) {
        ++calls;
        done(false, "runtime stopped");
        return RuntimeClient::RequestId(calls);
    };
    std::vector<RuntimeClient::RequestId> cancelled;
    {
        ProviderTest t(provider(false), req, [&](RuntimeClient::RequestId id) { cancelled.push_back(id); }, {}, 1);
        t.start();
        CHECK(t.state() == ProviderTest::State::Failed);
        CHECK_EQ(calls, 3);
    }
    CHECK(cancelled.empty());  // nothing was left open
}
