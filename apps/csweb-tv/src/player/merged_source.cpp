// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/merged_source.hpp"

#include "wnk/log.hpp"

#include <thread>

namespace csweb::player {

using namespace wnk::media;

namespace {
int64_t when(const Packet &p) { return p.dts_us != kNoTime ? p.dts_us : p.pts_us; }
}  // namespace

MergedSource::MergedSource(std::unique_ptr<MediaSource> video, std::vector<std::string> audio_uris,
                           std::vector<std::unique_ptr<MediaSource>> audio)
    : video_(std::move(video)), audio_uris_(std::move(audio_uris)), audio_(std::move(audio)), audio_open_(audio_.size(), false) {}

bool MergedSource::open(const std::string &uri, std::string *error) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < audio_.size() && i < audio_uris_.size(); i++) {
        threads.emplace_back([this, i] {
            std::string err;
            audio_open_[i] = audio_[i]->open(audio_uris_[i], &err);
            if (!audio_open_[i]) WNK_LOGW("csweb", "audio stream %zu not opened: %s", i, err.c_str());
        });
    }
    const bool ok = video_->open(uri, error);
    for (auto &t : threads) t.join();
    if (!ok) return false;

    info_ = video_->info();
    for (size_t i = 0; i < audio_.size(); i++) {
        if (!audio_open_[i]) continue;
        const MediaInfo &a = audio_[i]->info();
        info_.seekable = info_.seekable && a.seekable;
        for (TrackInfo t : a.tracks) {
            if (t.type != TrackType::Audio) continue;
            t.index += kAudioBase * static_cast<int>(i + 1);
            info_.tracks.push_back(std::move(t));
        }
    }
    // ExoPlayer's DefaultTrackSelector prefers more channels, then a higher sample rate, then a higher
    // bitrate. Bitrates are not known here; the plugin lists equal streams in rising quality (YouTube),
    // so a tie goes to the later one.
    TrackInfo *best = nullptr;
    for (TrackInfo &t : info_.tracks) {
        if (t.type != TrackType::Audio) continue;
        t.is_default = false;
        if (best == nullptr || t.channels > best->channels ||
            (t.channels == best->channels && t.sample_rate >= best->sample_rate)) {
            best = &t;
        }
    }
    if (best != nullptr) best->is_default = true;
    WNK_LOGI("csweb", "merged source: %zu of %zu audio stream(s) open", audio_streams_open(), audio_.size());
    return true;
}

size_t MergedSource::audio_streams_open() const {
    size_t n = 0;
    for (bool b : audio_open_) n += b ? 1 : 0;
    return n;
}

bool MergedSource::select(int video_track, int audio_track, const BackendCaps &caps, StreamFormat *out,
                          std::string *error) {
    StreamFormat vf;
    if (!video_->select(video_track, audio_track >= kAudioBase ? -1 : audio_track, caps, &vf, error)) return false;
    out->video = vf.video;
    out->audio = vf.audio;
    video_lane_ = Lane{video_.get(), {}, false};
    audio_lane_ = Lane{};
    if (audio_track >= kAudioBase) {
        const size_t n = static_cast<size_t>(audio_track / kAudioBase - 1);
        if (n >= audio_.size() || !audio_open_[n]) {
            *error = "no such audio track";
            return false;
        }
        StreamFormat af;
        if (!audio_[n]->select(-1, audio_track % kAudioBase, caps, &af, error)) return false;
        out->audio = af.audio;
        audio_lane_ = Lane{audio_[n].get(), {}, false};
    }
    return out->video || out->audio;
}

MediaSource::Read MergedSource::fill(Lane &lane, std::string *error) {
    if (lane.src == nullptr || lane.done || !lane.pending.empty()) return Read::Packet;
    Packet p;
    const Read r = lane.src->read(&p, error);
    if (r == Read::EndOfStream) {
        lane.done = true;
        return Read::Packet;
    }
    if (r != Read::Packet) return r;
    lane.pending.push_back(std::move(p));
    return Read::Packet;
}

MediaSource::Read MergedSource::read(Packet *out, std::string *error) {
    for (Lane *lane : {&video_lane_, &audio_lane_}) {
        const Read r = fill(*lane, error);
        if (r != Read::Packet) return r;
    }
    Lane *next = nullptr;
    for (Lane *lane : {&video_lane_, &audio_lane_}) {
        if (lane->pending.empty()) continue;
        if (next == nullptr || when(lane->pending.front()) == kNoTime ||
            (when(next->pending.front()) != kNoTime && when(lane->pending.front()) < when(next->pending.front()))) {
            next = lane;
        }
    }
    if (next == nullptr) return Read::EndOfStream;
    *out = std::move(next->pending.front());
    next->pending.pop_front();
    return Read::Packet;
}

bool MergedSource::seek(int64_t pts_us, std::string *error) {
    video_lane_.pending.clear();
    video_lane_.done = false;
    audio_lane_.pending.clear();
    audio_lane_.done = false;
    if (!video_->seek(pts_us, error)) return false;
    if (audio_lane_.src == nullptr) return true;
    // The audio starts where the video does: at its keyframe, which may be before pts_us.
    int64_t at = pts_us;
    if (video_lane_.src != nullptr) {
        for (int i = 0; i < 64; i++) {
            Packet p;
            const Read r = video_->read(&p, error);
            if (r == Read::EndOfStream) {
                video_lane_.done = true;
                break;
            }
            if (r == Read::Interrupted) {
                // Cut short by a newer command; like the demuxer's own seek this one succeeds and
                // read() reports the interrupt.
                std::string ignored;
                audio_lane_.src->seek(pts_us, &ignored);
                return true;
            }
            if (r != Read::Packet) return false;
            const bool video = p.type == TrackType::Video && p.pts_us != kNoTime;
            if (video) at = p.pts_us;
            video_lane_.pending.push_back(std::move(p));
            if (video) break;
        }
    }
    return audio_lane_.src->seek(at, error);
}

void MergedSource::interrupt() {
    video_->interrupt();
    for (auto &a : audio_) a->interrupt();
}

void MergedSource::reset_interrupt() {
    video_->reset_interrupt();
    for (auto &a : audio_) a->reset_interrupt();
}

}  // namespace csweb::player
