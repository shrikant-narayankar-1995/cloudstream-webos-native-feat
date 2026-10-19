// SPDX-License-Identifier: GPL-3.0-or-later
// player/merged_source: a video and separate audio streams as one source (ExtractorLink.audioTracks).
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "player/merged_source.hpp"

using namespace csweb::player;
using namespace wnk::media;

namespace {

// A stream of packets of one type every `step` µs; keyframes every `gop` packets.
struct FakeSource : MediaSource {
    TrackType type;
    int64_t step, gop, end;
    bool fail_open = false;
    MediaInfo mi;
    int64_t at = 0;
    std::string opened, selected;
    int64_t seeks_to = -1;

    FakeSource(TrackType t, int64_t step_us, int64_t gop_n, int64_t end_us) : type(t), step(step_us), gop(gop_n), end(end_us) {
        TrackInfo ti;
        ti.index = t == TrackType::Video ? 0 : 1;
        ti.type = t;
        ti.codec = t == TrackType::Video ? CodecId::H264 : CodecId::Aac;
        ti.channels = t == TrackType::Audio ? 2 : 0;
        ti.sample_rate = t == TrackType::Audio ? 44100 : 0;
        mi.tracks.push_back(ti);
        mi.seekable = true;
        mi.duration_us = end_us;
    }
    bool open(const std::string &uri, std::string *error) override {
        opened = uri;
        if (fail_open) *error = "404";
        return !fail_open;
    }
    const MediaInfo &info() const override { return mi; }
    bool select(int v, int a, const BackendCaps &, StreamFormat *out, std::string *) override {
        selected = std::to_string(v) + "," + std::to_string(a);
        if (v >= 0) {
            VideoFormat vf;
            vf.codec = CodecId::H264;
            out->video = vf;
        }
        if (a >= 0) {
            AudioFormat af;
            af.codec = CodecId::Aac;
            out->audio = af;
        }
        return true;
    }
    Read read(Packet *out, std::string *) override {
        if (at >= end) return Read::EndOfStream;
        out->type = type;
        out->pts_us = at;
        out->key = type == TrackType::Audio || (at / step) % gop == 0;
        at += step;
        return Read::Packet;
    }
    bool seek(int64_t pts, std::string *) override {
        seeks_to = pts;
        at = pts / (step * gop) * (step * gop);  // the keyframe at or before
        return true;
    }
    void interrupt() override {}
    void reset_interrupt() override {}
};

struct Fixture {
    FakeSource *video, *a0, *a1;
    std::unique_ptr<MergedSource> src;
    explicit Fixture(bool fail_first_audio = false) {
        auto v = std::make_unique<FakeSource>(TrackType::Video, 40000, 25, 4000000);
        auto x = std::make_unique<FakeSource>(TrackType::Audio, 23000, 1, 4000000);
        auto y = std::make_unique<FakeSource>(TrackType::Audio, 20000, 1, 4000000);
        x->fail_open = fail_first_audio;
        x->mi.tracks[0].sample_rate = 48000;  // the better stream first
        video = v.get();
        a0 = x.get();
        a1 = y.get();
        std::vector<std::unique_ptr<MediaSource>> audio;
        audio.push_back(std::move(x));
        audio.push_back(std::move(y));
        src = std::make_unique<MergedSource>(std::move(v), std::vector<std::string>{"https://a/0", "https://a/1"}, std::move(audio));
    }
};

}  // namespace

TEST("tracks_of_every_stream") {
    Fixture f;
    std::string error;
    CHECK(f.src->open("https://v", &error));
    CHECK_EQ(f.video->opened, std::string("https://v"));
    CHECK_EQ(f.a1->opened, std::string("https://a/1"));
    const MediaInfo &mi = f.src->info();
    CHECK_EQ(mi.tracks.size(), size_t(3));
    CHECK_EQ(mi.tracks[1].index, 1001);
    CHECK_EQ(mi.tracks[2].index, 2001);
    CHECK(mi.tracks[1].is_default);  // 48 kHz over 44.1 kHz
    CHECK(!mi.tracks[2].is_default);
    CHECK_EQ(mi.best(TrackType::Audio)->index, 1001);
    f.a0->mi.tracks[0].sample_rate = 44100;  // equal: the later stream
    CHECK(f.src->open("https://v", &error));
    CHECK_EQ(f.src->info().best(TrackType::Audio)->index, 2001);
}

TEST("interleaves_by_time") {
    Fixture f;
    std::string error;
    CHECK(f.src->open("https://v", &error));
    StreamFormat fmt;
    CHECK(f.src->select(0, 2001, BackendCaps{}, &fmt, &error));
    CHECK(fmt.video.has_value());
    CHECK(fmt.audio.has_value());
    CHECK_EQ(f.video->selected, std::string("0,-1"));
    CHECK_EQ(f.a1->selected, std::string("-1,1"));
    int64_t last = -1;
    int videos = 0, audios = 0;
    Packet p;
    while (f.src->read(&p, &error) == MediaSource::Read::Packet) {
        CHECK(p.pts_us >= last);
        last = p.pts_us;
        (p.type == TrackType::Video ? videos : audios)++;
    }
    CHECK_EQ(videos, 100);
    CHECK_EQ(audios, 200);
}

TEST("audio_follows_the_video_keyframe_on_seek") {
    Fixture f;
    std::string error;
    CHECK(f.src->open("https://v", &error));
    StreamFormat fmt;
    CHECK(f.src->select(0, 1001, BackendCaps{}, &fmt, &error));
    CHECK(f.src->seek(1500000, &error));
    CHECK_EQ(f.video->seeks_to, int64_t(1500000));
    CHECK_EQ(f.a0->seeks_to, int64_t(1000000));  // the keyframe the video resumes from
    Packet p;
    CHECK(f.src->read(&p, &error) == MediaSource::Read::Packet);
    CHECK(p.type == TrackType::Audio);
    CHECK_EQ(p.pts_us, int64_t(989000));  // the audio frame holding the keyframe's time
    CHECK(f.src->read(&p, &error) == MediaSource::Read::Packet);
    CHECK(p.type == TrackType::Video);
    CHECK(p.key);
    CHECK_EQ(p.pts_us, int64_t(1000000));
}

TEST("video_alone_when_audio_fails") {
    Fixture f(true);
    std::string error;
    CHECK(f.src->open("https://v", &error));
    CHECK_EQ(f.src->audio_streams_open(), size_t(1));
    CHECK_EQ(f.src->info().tracks.size(), size_t(2));
    CHECK_EQ(f.src->info().tracks[1].index, 2001);
    StreamFormat fmt;
    CHECK(!f.src->select(0, 1001, BackendCaps{}, &fmt, &error));
    CHECK(f.src->select(0, -1, BackendCaps{}, &fmt, &error));
    CHECK(!fmt.audio.has_value());
}
