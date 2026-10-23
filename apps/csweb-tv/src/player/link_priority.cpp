// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/link_priority.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <strings.h>

namespace csweb::player {

const char *const kCloudStreamUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

namespace {
struct Q {
    int value, priority;
};
// Qualities, in declaration order (minBy keeps the first of equals).
constexpr Q kQualities[] = {{400, 4}, {144, 0}, {240, 2}, {360, 3}, {480, 4}, {720, 5}, {1080, 6}, {1440, 7}, {2160, 8}};

bool starts_with_ci(const std::string &s, const char *prefix) {
    return strncasecmp(s.c_str(), prefix, std::char_traits<char>::length(prefix)) == 0;
}

bool contains_ci(const std::string &str, const std::string &sub) {
    if (str.empty() || sub.empty() || sub.size() > str.size()) return false;
    auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != str.end();
}

bool contains_word_ci(const std::string &str, const std::string &word) {
    if (str.empty() || word.empty() || word.size() > str.size()) return false;
    auto it = str.begin();
    while (it != str.end()) {
        it = std::search(it, str.end(), word.begin(), word.end(),
                         [](char a, char b) {
                             return std::tolower(static_cast<unsigned char>(a)) ==
                                    std::tolower(static_cast<unsigned char>(b));
                         });
        if (it == str.end()) return false;
        const bool start_ok = (it == str.begin()) || !std::isalnum(static_cast<unsigned char>(*(it - 1)));
        auto after = it + word.length();
        const bool end_ok = (after == str.end()) || !std::isalnum(static_cast<unsigned char>(*after));
        if (start_ok && end_ok) return true;
        it++;
    }
    return false;
}
}  // namespace

int quality_priority(int quality) {
    const Q *best = &kQualities[0];
    for (const Q &q : kQualities) {
        if (std::abs(q.value - quality) < std::abs(best->value - quality)) best = &q;
    }
    return best->priority;
}

bool is_dolby_vision(const runtime::Link &link) {
    auto match = [](const std::string &s) {
        if (s.empty()) return false;
        if (contains_ci(s, "dolby vision") || contains_ci(s, "dolby-vision") || contains_ci(s, "dolbyvision")) return true;
        if (contains_ci(s, "dovi")) return true;
        if (contains_word_ci(s, "dv")) return true;
        return false;
    };
    return match(link.name) || match(link.source);
}

bool is_dolby_atmos(const runtime::Link &link) {
    auto match = [](const std::string &s) {
        if (s.empty()) return false;
        if (contains_ci(s, "atmos")) return true;
        return false;
    };
    if (match(link.name) || match(link.source)) return true;
    for (const auto &track : link.audio_tracks) {
        if (match(track.url)) return true;
    }
    return false;
}

int link_priority(const runtime::Link &link, const LinkPreferences &prefs) {
    int p = quality_priority(link.quality) + kDefaultSourcePriority;
    if (prefs.prefer_dolby_vision && is_dolby_vision(link)) p += 2;
    if (prefs.prefer_dolby_atmos && is_dolby_atmos(link)) p += 1;
    return p;
}

std::string unsupported_reason(const runtime::Link &link) {
    if (link.link_type == "TORRENT" || link.link_type == "MAGNET") return "torrent";
    if (!starts_with_ci(link.url, "http://") && !starts_with_ci(link.url, "https://")) return "not a web address";
    if (link.drm && !link.drm->clearkey()) return std::string(runtime::drm_name(link.drm->uuid)) + " DRM";
    if (link.drm && (link.drm->kid.empty() || link.drm->key.empty()) && link.drm->license_url.empty()) return "ClearKey without a key";
    return {};
}

bool playable(const runtime::Link &link) { return unsupported_reason(link).empty(); }

std::vector<size_t> sort_links(const std::vector<runtime::Link> &links, const LinkPreferences &prefs) {
    std::vector<size_t> order(links.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&links, &prefs](size_t a, size_t b) {
                         return link_priority(links[a], prefs) > link_priority(links[b], prefs);
                     });
    return order;
}

std::vector<std::pair<std::string, std::string>> link_headers(const runtime::Link &link) {
    std::vector<std::pair<std::string, std::string>> out(link.headers.begin(), link.headers.end());
    auto has = [&out](const char *name) {
        return std::any_of(out.begin(), out.end(), [name](const auto &h) { return strcasecmp(h.first.c_str(), name) == 0; });
    };
    if (!link.referer.empty() && !has("referer")) out.emplace_back("referer", link.referer);
    if (!has("user-agent")) out.emplace_back("User-Agent", kCloudStreamUserAgent);
    return out;
}

}  // namespace csweb::player
