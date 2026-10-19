// SPDX-License-Identifier: GPL-3.0-or-later
//
// What plugins return, as plain structs parsed from the runtime's events. Field names follow
// CloudStream's own classes (SearchResponse, LoadResponse, Episode, ExtractorLink, SubtitleFile) so a
// reader can map one to the other; anything a plugin leaves out stays empty or zero.
#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace csweb::runtime {

struct Provider {
    std::string name, plugin, main_url, lang;
    bool has_main_page = false;
    std::vector<std::string> types;      // TvType names: Movie, TvSeries, Anime, Live, Others...
    std::vector<std::string> main_page;  // section names, in order (an index is a "section" request)
};

struct SearchResult {
    std::string name, url, api_name, type, poster_url, quality;
    int year = 0;
};

struct Actor {
    std::string name, image, role;
};

struct Episode {
    std::string name, data, poster_url, description, dub, score;
    int season = 0, episode = 0, run_time = 0;
    long long date = 0;
};

struct Season {
    int season = 0;
    std::string name;
};

struct Details {
    std::string name, url, api_name, type, poster_url, background_url, logo_url, plot, score, content_rating,
        show_status, data_url;
    int year = 0, duration = 0;
    bool coming_soon = false;
    std::vector<std::string> tags, trailers;
    std::vector<Actor> actors;
    std::vector<SearchResult> recommendations;
    std::vector<Season> seasons;
    std::vector<Episode> episodes;
    // Movies (and other single-item types) carry the data for loadLinks themselves.
    bool is_episodic() const { return !episodes.empty() || data_url.empty(); }
};

struct Link {
    std::string name, source, url, referer, link_type;  // link_type: VIDEO, M3U8, DASH, TORRENT, MAGNET
    int quality = 400;                                   // CloudStream Qualities value (400 unknown, 0 auto, 1080...)
    std::map<std::string, std::string> headers;          // only with withHeaders; never logged
};

struct Subtitle {
    std::string lang, url;
};

Provider parse_provider(const nlohmann::json &e);
SearchResult parse_search_result(const nlohmann::json &e);
Episode parse_episode(const nlohmann::json &e);
Details parse_details(const nlohmann::json &e);
Link parse_link(const nlohmann::json &e);
Subtitle parse_subtitle(const nlohmann::json &e);

// CloudStream Qualities.getStringByInt: "Auto" (0), "" (400, unknown), "4K" (2160), else "<n>p".
std::string quality_label(int quality);

}  // namespace csweb::runtime
