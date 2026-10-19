// SPDX-License-Identifier: GPL-3.0-or-later
//
// A link's video together with its separate audio streams (ExtractorLink.audioTracks), as CloudStream's
// CS3IPlayer merges them (MergingMediaSource): YouTube links, for one, are video-only streams with the
// audio beside them. One source per stream; the video's tracks keep their indices, the audio tracks of
// stream n are numbered kAudioBase * (n + 1) + their own index. open() opens the streams in parallel,
// marks the audio track ExoPlayer would choose as the default, and plays the video alone when no audio
// stream opens. read() interleaves the video and the selected audio
// by decode time; seek() moves the audio to where the video resumes (its keyframe).
#pragma once

#include "wnk/media/media.hpp"

#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace csweb::player {

class MergedSource : public wnk::media::MediaSource {
 public:
    static constexpr int kAudioBase = 1000;

    MergedSource(std::unique_ptr<wnk::media::MediaSource> video, std::vector<std::string> audio_uris,
                 std::vector<std::unique_ptr<wnk::media::MediaSource>> audio);

    bool open(const std::string &uri, std::string *error) override;
    const wnk::media::MediaInfo &info() const override { return info_; }
    bool select(int video_track, int audio_track, const wnk::media::BackendCaps &caps, wnk::media::StreamFormat *out,
                std::string *error) override;
    Read read(wnk::media::Packet *out, std::string *error) override;
    bool seek(int64_t pts_us, std::string *error) override;
    void interrupt() override;
    void reset_interrupt() override;

    size_t audio_streams_open() const;

 private:
    struct Lane {
        wnk::media::MediaSource *src = nullptr;
        std::deque<wnk::media::Packet> pending;
        bool done = false;
    };
    Read fill(Lane &lane, std::string *error);

    std::unique_ptr<wnk::media::MediaSource> video_;
    std::vector<std::string> audio_uris_;
    std::vector<std::unique_ptr<wnk::media::MediaSource>> audio_;
    std::vector<bool> audio_open_;
    wnk::media::MediaInfo info_;
    Lane video_lane_;
    Lane audio_lane_;  // src null: no audio selected
};

}  // namespace csweb::player
