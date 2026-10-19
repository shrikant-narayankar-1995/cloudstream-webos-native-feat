// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/runtime_types.hpp"

namespace csweb::runtime {

namespace {

using nlohmann::json;

std::string str(const json &e, const char *key) {
    auto it = e.find(key);
    if (it == e.end() || it->is_null()) return {};
    if (it->is_string()) return it->get<std::string>();
    return it->dump();
}

long long num(const json &e, const char *key, long long def = 0) {
    auto it = e.find(key);
    if (it == e.end()) return def;
    if (it->is_number_integer()) return it->get<long long>();
    if (it->is_number()) return static_cast<long long>(it->get<double>());
    if (it->is_string()) {  // the runtime sends some numbers as strings (String.valueOf)
        try {
            return std::stoll(it->get<std::string>());
        } catch (...) {
            return def;
        }
    }
    return def;
}

std::vector<std::string> strings(const json &e, const char *key) {
    std::vector<std::string> out;
    auto it = e.find(key);
    if (it == e.end() || !it->is_array()) return out;
    for (const auto &v : *it) {
        if (v.is_string()) out.push_back(v.get<std::string>());
    }
    return out;
}

}  // namespace

Provider parse_provider(const json &e) {
    Provider p;
    p.name = str(e, "name");
    p.plugin = str(e, "plugin");
    p.main_url = str(e, "mainUrl");
    p.lang = str(e, "lang");
    p.has_main_page = e.value("hasMainPage", false);
    p.types = strings(e, "supportedTypes");
    p.main_page = strings(e, "mainPage");
    return p;
}

SearchResult parse_search_result(const json &e) {
    SearchResult r;
    r.name = str(e, "name");
    r.url = str(e, "url");
    r.api_name = str(e, "apiName");
    r.type = str(e, "type");
    r.poster_url = str(e, "posterUrl");
    r.quality = str(e, "quality");
    r.year = static_cast<int>(num(e, "year"));
    return r;
}

Episode parse_episode(const json &e) {
    Episode ep;
    ep.name = str(e, "name");
    ep.data = str(e, "data");
    ep.poster_url = str(e, "posterUrl");
    ep.description = str(e, "description");
    ep.dub = str(e, "dub");
    ep.score = str(e, "score");
    ep.season = static_cast<int>(num(e, "season"));
    ep.episode = static_cast<int>(num(e, "episode"));
    ep.run_time = static_cast<int>(num(e, "runTime"));
    ep.date = num(e, "date");
    return ep;
}

Details parse_details(const json &e) {
    Details d;
    d.name = str(e, "name");
    d.url = str(e, "url");
    d.api_name = str(e, "apiName");
    d.type = str(e, "type");
    d.poster_url = str(e, "posterUrl");
    d.background_url = str(e, "backgroundPosterUrl");
    d.logo_url = str(e, "logoUrl");
    d.plot = str(e, "plot");
    d.score = str(e, "score");
    d.content_rating = str(e, "contentRating");
    d.show_status = str(e, "showStatus");
    d.data_url = str(e, "dataUrl");
    d.year = static_cast<int>(num(e, "year"));
    d.duration = static_cast<int>(num(e, "duration"));
    d.coming_soon = e.value("comingSoon", false);
    d.tags = strings(e, "tags");
    if (auto it = e.find("trailers"); it != e.end() && it->is_array()) {
        for (const auto &t : *it) {
            if (t.is_string()) d.trailers.push_back(t.get<std::string>());
        }
    }
    if (auto it = e.find("actors"); it != e.end() && it->is_array()) {
        for (const auto &a : *it) {
            if (a.is_object()) d.actors.push_back({str(a, "name"), str(a, "image"), str(a, "role")});
        }
    }
    if (auto it = e.find("recommendations"); it != e.end() && it->is_array()) {
        for (const auto &r : *it) {
            if (r.is_object()) d.recommendations.push_back(parse_search_result(r));
        }
    }
    if (auto it = e.find("seasons"); it != e.end() && it->is_array()) {
        for (const auto &s : *it) {
            if (!s.is_object()) continue;
            Season season;
            season.season = static_cast<int>(num(s, "season"));
            season.name = str(s, "name");
            d.seasons.push_back(season);
        }
    }
    return d;
}

Link parse_link(const json &e) {
    Link l;
    l.name = str(e, "name");
    l.source = str(e, "source");
    l.url = str(e, "url");
    l.referer = str(e, "referer");
    l.link_type = str(e, "linkType");
    l.quality = static_cast<int>(num(e, "quality", 400));
    if (auto it = e.find("headers"); it != e.end() && it->is_object()) {
        for (auto h = it->begin(); h != it->end(); ++h) {
            if (h.value().is_string()) l.headers[h.key()] = h.value().get<std::string>();
        }
    }
    return l;
}

Subtitle parse_subtitle(const json &e) { return {str(e, "lang"), str(e, "url")}; }

std::string quality_label(int quality) {
    if (quality == 0) return "Auto";
    if (quality == 400) return {};
    if (quality == 2160) return "4K";
    return std::to_string(quality) + "p";
}

}  // namespace csweb::runtime
