// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which link plays first, as CloudStream's PlayerGeneratorViewModel.sortLinks orders them with the
// default quality profile (QualityDataHelper.getLinkPriority): the priority of the Qualities entry
// closest to the link's quality (144p 0 … 2160p 8, unknown 4) plus the source priority (1 unless
// the user changed it), highest first, ties in arrival order. A link at AUTO_SKIP_PRIORITY or above
// starts playback before loading ends.
#pragma once

#include "runtime/runtime_types.hpp"

#include <string>
#include <vector>

namespace csweb::player {

inline constexpr int kAutoSkipPriority = 10;
inline constexpr int kDefaultSourcePriority = 1;

int quality_priority(int quality);
int link_priority(const runtime::Link &link);
// Torrents and magnets need CloudStream's torrent server, which the app does not have; anything
// that is not http(s) cannot be opened either.
bool playable(const runtime::Link &link);
// Indices into `links`, best first.
std::vector<size_t> sort_links(const std::vector<runtime::Link> &links);

// The request headers for a link (ExtractorLink.getAllHeaders: its headers plus Referer when it has
// one and no header names it), with CloudStream's player User-Agent when the link sets none.
std::vector<std::pair<std::string, std::string>> link_headers(const runtime::Link &link);
// MainAPI.USER_AGENT of the CloudStream version the runtime bundles.
extern const char *const kCloudStreamUserAgent;

}  // namespace csweb::player
