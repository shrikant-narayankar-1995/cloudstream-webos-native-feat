// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/episode_groups.hpp"

#include <algorithm>
#include <climits>

namespace csweb::runtime {

std::map<EpisodeGroup, std::vector<Episode>> group_episodes(const std::vector<Episode> &eps) {
    std::map<EpisodeGroup, std::vector<Episode>> out;
    for (size_t i = 0; i < eps.size(); i++) {
        Episode ep = eps[i];
        if (ep.episode <= 0) ep.episode = static_cast<int>(i) + 1;  // `episode ?: index + 1`
        out[{ep.dub, ep.season}].push_back(std::move(ep));
    }
    return out;
}

std::vector<EpisodeRange> episode_ranges(const std::vector<Episode> &eps, int size) {
    std::vector<EpisodeRange> list;
    if (eps.empty()) return list;
    if (eps.size() <= static_cast<size_t>(size + 10)) {  // EPISODE_RANGE_OVERLOAD
        int lo = INT_MAX, hi = INT_MIN;
        for (const auto &e : eps) lo = std::min(lo, e.episode), hi = std::max(hi, e.episode);
        list.push_back({0, eps.size(), lo, hi});
        return list;
    }
    size_t index = 0;
    int target = 0;
    int lo = eps.front().episode, hi = eps.front().episode;
    while (index < eps.size()) {
        const size_t start = index;
        target += size;
        while (index < eps.size() && eps[index].episode <= target) {
            lo = std::min(lo, eps[index].episode);
            hi = std::max(hi, eps[index].episode);
            ++index;
        }
        if (index == start) continue;
        list.push_back({start, index - start, lo, hi});
        lo = INT_MAX;
        hi = INT_MIN;
    }
    return list;
}

}  // namespace csweb::runtime
