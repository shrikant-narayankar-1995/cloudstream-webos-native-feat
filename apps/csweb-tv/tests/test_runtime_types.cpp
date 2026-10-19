// SPDX-License-Identifier: GPL-3.0-or-later
// runtime/runtime_types on events as the runtime prints them (captured from host runs).
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "runtime/runtime_types.hpp"

using namespace csweb::runtime;
using nlohmann::json;

TEST("provider_and_search") {
    auto p = parse_provider(json::parse(R"J({"event":"provider","name":"Librivox Audiobook","plugin":"/x/L.cs3",
        "mainUrl":"https://librivox.org","lang":"en","hasMainPage":true,"supportedTypes":["Others"],"mainPage":["Latest Audiobook"]})J"));
    CHECK_EQ(p.name, std::string("Librivox Audiobook"));
    CHECK(p.has_main_page);
    CHECK_EQ(p.main_page.size(), size_t(1));
    auto r = parse_search_result(json::parse(R"J({"event":"result","name":"Metropolis","url":"u","apiName":"IA",
        "type":"Movie","posterUrl":"p","year":1927})J"));
    CHECK_EQ(r.year, 1927);
    CHECK_EQ(r.type, std::string("Movie"));
}

TEST("details_full") {
    auto d = parse_details(json::parse(R"J({"event":"details","name":"N","url":"u","type":"TvSeries","year":"2019",
        "score":"8.3","duration":42,"tags":["Drama"],"actors":[{"name":"A","image":null,"role":"MAIN"}],
        "recommendations":[{"name":"R","url":"r"}],"trailers":["t"],"seasons":[{"season":1,"name":"One"}],
        "backgroundPosterUrl":"b","comingSoon":false})J"));
    CHECK_EQ(d.year, 2019);
    CHECK_EQ(d.score, std::string("8.3"));
    CHECK_EQ(d.actors.size(), size_t(1));
    CHECK_EQ(d.actors[0].image, std::string());
    CHECK_EQ(d.recommendations[0].name, std::string("R"));
    CHECK_EQ(d.seasons[0].name, std::string("One"));
    CHECK(d.is_episodic());
    auto m = parse_details(json::parse(R"J({"name":"M","dataUrl":"data"})J"));
    CHECK(!m.is_episodic());
}

TEST("links_and_quality") {
    auto l = parse_link(json::parse(R"J({"event":"link","name":"L","url":"https://x/a.m3u8","quality":1080,
        "linkType":"M3U8","referer":"https://x/","headers":{"User-Agent":"ua"}})J"));
    CHECK_EQ(l.quality, 1080);
    CHECK_EQ(l.headers.at("User-Agent"), std::string("ua"));
    CHECK_EQ(parse_link(json::parse(R"J({"name":"x"})J")).quality, 400);
    CHECK_EQ(quality_label(1080), std::string("1080p"));
    CHECK_EQ(quality_label(2160), std::string("4K"));
    CHECK_EQ(quality_label(400), std::string());
    CHECK_EQ(quality_label(0), std::string("Auto"));
    auto e = parse_episode(json::parse(R"J({"name":"E","data":"d","season":2,"episode":4,"dub":"DUBBED"})J"));
    CHECK_EQ(e.episode, 4);
    CHECK_EQ(e.dub, std::string("DUBBED"));
}
