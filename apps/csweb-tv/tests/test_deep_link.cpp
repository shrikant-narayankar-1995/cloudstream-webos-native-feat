// SPDX-License-Identifier: GPL-3.0-or-later
// app/deep_link against MainActivity.handleAppIntentUrl's schemes.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "app/deep_link.hpp"

using namespace csweb::app;

TEST("repository_links") {
    auto a = parse_deep_link(R"({"target":"cloudstreamrepo://example.org/repo.json"})");
    CHECK(a.kind == DeepLink::Kind::Repository);
    CHECK_EQ(a.value, std::string("https://example.org/repo.json"));
    auto b = parse_deep_link(R"({"url":"https://cs.repo/?example.org/r.json"})");
    CHECK(b.kind == DeepLink::Kind::Repository);
    CHECK_EQ(b.value, std::string("https://example.org/r.json"));
    CHECK(parse_deep_link(R"({"target":"https://cs.repo"})").kind == DeepLink::Kind::None);
}

TEST("search_link") {
    auto s = parse_deep_link(R"({"target":"cloudstreamsearch://pride%20and+prejudice"})");
    CHECK(s.kind == DeepLink::Kind::Search);
    CHECK_EQ(s.value, std::string("pride and prejudice"));
}

TEST("ignored") {
    CHECK(parse_deep_link("").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link("{}").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link("not json").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link(R"({"target":"https://example.org"})").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link(R"({"target":42})").kind == DeepLink::Kind::None);
}
