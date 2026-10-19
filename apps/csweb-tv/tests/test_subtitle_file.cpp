// SPDX-License-Identifier: GPL-3.0-or-later
// player/subtitle_file: WebVTT and SubRip files as plugins return them.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "player/subtitle_file.hpp"

using namespace csweb::player;
using wnk::media::SubtitleCue;

TEST("srt_with_bom_and_crlf") {
    const std::string srt = "\xEF\xBB\xBF" "1\r\n00:00:01,500 --> 00:00:03,000\r\nHello <i>there</i>\r\n\r\n"
                            "2\r\n00:00:02,000 --> 00:00:04,250\r\nSecond\r\n";
    std::vector<SubtitleCue> cues;
    std::string error;
    REQUIRE(parse_subtitle_file(srt, &cues, &error));
    REQUIRE(cues.size() == 2);
    CHECK_EQ(cues[0].start_us, int64_t(1500000));
    CHECK_EQ(cues[1].end_us, int64_t(4250000));
    CHECK_EQ(cues[0].text, std::string("Hello there"));
}

TEST("vtt_and_cues_at") {
    const std::string vtt = "WEBVTT\n\n00:05.000 --> 00:07.000\nB\n\n00:01.000 --> 00:02.000\nA\n";
    std::vector<SubtitleCue> cues;
    std::string error;
    REQUIRE(parse_subtitle_file(vtt, &cues, &error));
    REQUIRE(cues.size() == 2);
    CHECK_EQ(cues[0].text, std::string("A"));  // sorted by start
    std::vector<std::shared_ptr<const SubtitleCue>> shared;
    for (auto &c : cues) shared.push_back(std::make_shared<const SubtitleCue>(c));
    CHECK_EQ(cues_at(shared, 1500000).size(), size_t(1));
    CHECK_EQ(cues_at(shared, 3000000).size(), size_t(0));
    CHECK_EQ(cues_at(shared, 6000000)[0]->text, std::string("B"));
}

TEST("not_subtitles") {
    std::vector<SubtitleCue> cues;
    std::string error;
    CHECK(!parse_subtitle_file("", &cues, &error));
    CHECK(!parse_subtitle_file("<html>nope</html>", &cues, &error));
}
