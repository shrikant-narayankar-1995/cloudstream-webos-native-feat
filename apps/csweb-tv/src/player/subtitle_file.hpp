// SPDX-License-Identifier: GPL-3.0-or-later
//
// Subtitle files a plugin hands over (SubtitleFile: a language and a URL), read into cues the
// SubtitleView can draw. WebVTT is read by the kit; SubRip (the other format plugins commonly
// return) differs from it only in the header and the decimal comma, so it is converted first.
#pragma once

#include "wnk/media/media.hpp"

#include <memory>
#include <string>
#include <vector>

namespace csweb::player {

using Cues = std::vector<std::shared_ptr<const wnk::media::SubtitleCue>>;

// False (with a reason) if the text is neither WebVTT nor SubRip.
bool parse_subtitle_file(const std::string &text, std::vector<wnk::media::SubtitleCue> *out, std::string *error);

// The cues showing at `position_us` (cues sorted by start).
Cues cues_at(const std::vector<std::shared_ptr<const wnk::media::SubtitleCue>> &cues, int64_t position_us);

}  // namespace csweb::player
