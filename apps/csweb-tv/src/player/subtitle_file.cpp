// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/subtitle_file.hpp"

#include "wnk/media/webvtt.hpp"

#include <algorithm>
#include <regex>

namespace csweb::player {

namespace {
std::string srt_to_vtt(const std::string &srt) {
    // "00:01:02,345 --> 00:01:04,000" → "00:01:02.345 --> 00:01:04.000"
    static const std::regex ts(R"((\d{1,2}:\d{2}:\d{2}),(\d{3}))");
    return "WEBVTT\n\n" + std::regex_replace(srt, ts, "$1.$2");
}
}  // namespace

bool parse_subtitle_file(const std::string &input, std::vector<wnk::media::SubtitleCue> *out, std::string *error) {
    std::string text = input;
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);  // UTF-8 BOM
    std::string normalized;
    normalized.reserve(text.size());
    for (char c : text) {
        if (c != '\r') normalized += c;
    }
    const size_t first = normalized.find_first_not_of(" \n\t");
    if (first == std::string::npos) {
        *error = "empty subtitle file";
        return false;
    }
    if (normalized.compare(first, 6, "WEBVTT") != 0) normalized = srt_to_vtt(normalized.substr(first));
    wnk::media::WebVttFile file;
    if (!wnk::media::parse_webvtt(normalized, &file, error)) return false;
    if (file.cues.empty()) {
        *error = "no cues";
        return false;
    }
    *out = std::move(file.cues);
    std::stable_sort(out->begin(), out->end(), [](const auto &a, const auto &b) { return a.start_us < b.start_us; });
    return true;
}

Cues cues_at(const std::vector<std::shared_ptr<const wnk::media::SubtitleCue>> &cues, int64_t position_us) {
    Cues now;
    if (position_us == wnk::media::kNoTime) return now;
    for (const auto &c : cues) {
        if (c->start_us > position_us) break;
        if (c->end_us == wnk::media::kNoTime || c->end_us > position_us) now.push_back(c);
    }
    return now;
}

}  // namespace csweb::player
