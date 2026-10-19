// SPDX-License-Identifier: GPL-3.0-or-later
//
// How CloudStream's result page arranges a series' episodes (ResultViewModel2): grouped by
// (dub status, season) — EpisodeIndexer — with a missing episode number taken as its position + 1,
// and a group of more than size + 10 episodes split into ranges of `size` by episode number
// (getRanges; the TV page uses 50).
#pragma once

#include "runtime/runtime_types.hpp"

#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace csweb::runtime {

struct EpisodeGroup {
    std::string dub;  // "" (none), else the DubStatus name ("Dubbed", "Subbed")
    int season = 0;
    bool operator<(const EpisodeGroup &o) const { return std::tie(dub, season) < std::tie(o.dub, o.season); }
};

struct EpisodeRange {
    size_t start = 0, length = 0;
    int min_episode = 0, max_episode = 0;
};

std::map<EpisodeGroup, std::vector<Episode>> group_episodes(const std::vector<Episode> &episodes);
std::vector<EpisodeRange> episode_ranges(const std::vector<Episode> &episodes, int size = 50);

}  // namespace csweb::runtime
