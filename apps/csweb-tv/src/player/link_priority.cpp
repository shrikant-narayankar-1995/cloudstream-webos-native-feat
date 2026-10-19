// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/link_priority.hpp"

#include <algorithm>
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
}  // namespace

int quality_priority(int quality) {
    const Q *best = &kQualities[0];
    for (const Q &q : kQualities) {
        if (std::abs(q.value - quality) < std::abs(best->value - quality)) best = &q;
    }
    return best->priority;
}

int link_priority(const runtime::Link &link) { return quality_priority(link.quality) + kDefaultSourcePriority; }

bool playable(const runtime::Link &link) {
    if (link.link_type == "TORRENT" || link.link_type == "MAGNET") return false;
    return starts_with_ci(link.url, "http://") || starts_with_ci(link.url, "https://");
}

std::vector<size_t> sort_links(const std::vector<runtime::Link> &links) {
    std::vector<size_t> order(links.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&links](size_t a, size_t b) { return link_priority(links[a]) > link_priority(links[b]); });
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
