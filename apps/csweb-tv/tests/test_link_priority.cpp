// SPDX-License-Identifier: GPL-3.0-or-later
// player/link_priority against CloudStream's defaults (Qualities.defaultPriority, source priority 1).
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "player/link_priority.hpp"
#include "player/media_factory.hpp"

using namespace csweb::player;
using csweb::runtime::Link;

namespace {
Link link(const std::string &name, int quality, std::string type = "VIDEO", std::string url = "https://h/v.mp4") {
    Link l;
    l.name = name;
    l.quality = quality;
    l.link_type = std::move(type);
    l.url = std::move(url);
    return l;
}
}  // namespace

TEST("quality_priorities") {
    CHECK_EQ(quality_priority(1080), 6);
    CHECK_EQ(quality_priority(2160), 8);
    CHECK_EQ(quality_priority(400), 4);   // Unknown
    CHECK_EQ(quality_priority(0), 0);     // Auto is closest to 144p
    CHECK_EQ(quality_priority(1000), 6);  // closest: 1080
    CHECK_EQ(quality_priority(420), 4);   // closest: Unknown (400)
    CHECK_EQ(link_priority(link("a", 720)), 6);
}

TEST("sort_best_first_stable") {
    std::vector<Link> links{link("a", 480), link("b", 1080), link("c", 400), link("d", 1080)};
    auto order = sort_links(links);
    REQUIRE(order.size() == 4);
    CHECK_EQ(order[0], size_t(1));
    CHECK_EQ(order[1], size_t(3));
    CHECK_EQ(order[2], size_t(0));  // 480 and unknown tie at 4+1: arrival order
    CHECK_EQ(order[3], size_t(2));
}

TEST("dolby_tag_detection") {
    CHECK(is_dolby_vision(link("Movie 4K DV", 2160)));
    CHECK(is_dolby_vision(link("Movie 4K DoVi HDR", 2160)));
    CHECK(is_dolby_vision(link("Movie 4K Dolby Vision", 2160)));
    CHECK(is_dolby_vision(link("Movie 4K Dolby-Vision", 2160)));
    CHECK(!is_dolby_vision(link("Movie 4K HDR", 2160)));
    CHECK(!is_dolby_vision(link("Advocate 1080p", 1080)));

    CHECK(is_dolby_atmos(link("Movie 4K Atmos", 2160)));
    CHECK(is_dolby_atmos(link("Movie 4K Dolby Atmos", 2160)));
    CHECK(!is_dolby_atmos(link("Movie 4K 5.1", 2160)));

    Link audio_atmos = link("Movie 1080p", 1080);
    audio_atmos.audio_tracks.push_back({.url = "https://h/stream_atmos.m4a"});
    CHECK(is_dolby_atmos(audio_atmos));
}

TEST("sort_with_dolby_preferences") {
    Link sdr = link("Stream SDR", 2160);
    Link dv = link("Stream 4K DV", 2160);
    Link atmos = link("Stream 4K Atmos", 2160);
    Link dv_atmos = link("Stream 4K DV Atmos", 2160);
    std::vector<Link> links{sdr, dv, atmos, dv_atmos};

    LinkPreferences both_on{true, true};
    auto order = sort_links(links, both_on);
    // dv_atmos (8+1+2+1=12) > dv (8+1+2=11) > atmos (8+1+1=10) > sdr (8+1=9)
    REQUIRE(order.size() == 4);
    CHECK_EQ(order[0], size_t(3));
    CHECK_EQ(order[1], size_t(1));
    CHECK_EQ(order[2], size_t(2));
    CHECK_EQ(order[3], size_t(0));

    LinkPreferences dv_off{false, true};
    order = sort_links(links, dv_off);
    // dv_atmos (8+1+1=10), atmos (8+1+1=10) tie; then sdr and dv tie at 9
    REQUIRE(order.size() == 4);
    CHECK_EQ(order[0], size_t(2));  // atmos arrived before dv_atmos
    CHECK_EQ(order[1], size_t(3));
    CHECK_EQ(order[2], size_t(0));  // sdr arrived before dv
    CHECK_EQ(order[3], size_t(1));
}

TEST("playable") {
    CHECK(playable(link("a", 720)));
    CHECK(!playable(link("t", 720, "TORRENT")));
    CHECK(!playable(link("m", 720, "MAGNET", "magnet:?xt=urn")));
    CHECK(!playable(link("f", 720, "VIDEO", "file:///etc/passwd")));
    CHECK(playable(link("h", 720, "M3U8", "HTTPS://h/x.m3u8")));
}

TEST("drm_links") {
    Link ck = link("ck", 1080, "DASH", "https://h/m.mpd");
    ck.drm = csweb::runtime::Drm{};
    ck.drm->uuid = "e2719d58-a985-b3c9-781a-b030af78d30e";
    CHECK(!playable(ck));  // ClearKey needs its key (or a licence server)
    CHECK_EQ(unsupported_reason(ck), std::string("ClearKey without a key"));
    ck.drm->kid = "_u3wDe7erb7v8Lqt8A3QDQ";  // JWK base64url, as CloudStream's player takes them
    ck.drm->key = "ABEiM0RVZneImaq7zN3u_w";
    CHECK(playable(ck));
    const auto keys = clearkey_keys(ck);
    REQUIRE(keys.size() == 1);
    CHECK_EQ(keys.begin()->first, std::string("feedf00deedeadbeeff0baadf00dd00d"));
    CHECK_EQ(keys.begin()->second, std::string("00112233445566778899aabbccddeeff"));
    ck.drm->key = "not a key";
    CHECK(clearkey_keys(ck).empty());
    Link wv = link("wv", 1080, "DASH", "https://h/m.mpd");
    wv.drm = csweb::runtime::Drm{};
    wv.drm->uuid = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
    wv.drm->license_url = "https://lic/wv";
    CHECK(!playable(wv));
    CHECK_EQ(unsupported_reason(wv), std::string("Widevine DRM"));
    CHECK(clearkey_keys(wv).empty());
}

TEST("headers") {
    Link l = link("a", 720);
    l.referer = "https://site/";
    auto h = link_headers(l);
    REQUIRE(h.size() == 2);
    CHECK_EQ(h[0].first, std::string("referer"));
    CHECK_EQ(h[1].first, std::string("User-Agent"));
    l.headers["Referer"] = "https://other/";
    l.headers["user-agent"] = "UA";
    h = link_headers(l);
    CHECK_EQ(h.size(), size_t(2));  // the link's own Referer and User-Agent win
}
