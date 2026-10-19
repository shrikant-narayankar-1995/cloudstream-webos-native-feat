// SPDX-License-Identifier: GPL-3.0-or-later
#include "player/media_factory.hpp"

#include "player/link_priority.hpp"

#include "wnk/log.hpp"
#include "wnk/media/adaptive_source.hpp"
#include "wnk/media/auto_backend.hpp"
#include "wnk/media/ffmpeg_source.hpp"
#include "wnk/media/ndl_backend.hpp"
#include "wnk/media/pacing_backend.hpp"
#include "wnk/media/starfish_backend.hpp"
#include "wnk/platform/process.hpp"

namespace csweb::player {

using namespace wnk::media;

std::unique_ptr<MediaBackend> make_backend(const DeviceMediaCaps &device) {
    if (!wnk::platform::kIsWebOS) return std::make_unique<PacingBackend>();
    std::string why_sf, why_ndl;
    const bool starfish = StarfishBackend::available(&why_sf);
    const bool ndl = NdlBackend::available(&why_ndl);
    StarfishOptions so;
    so.dolby_vision = device.dolby_vision == true;
    so.atmos = device.dolby_atmos == true;
    if (starfish && ndl) return std::make_unique<AutoBackend>(std::make_unique<StarfishBackend>(so), std::make_unique<NdlBackend>());
    if (starfish) return std::make_unique<StarfishBackend>(so);
    if (ndl) return std::make_unique<NdlBackend>();
    WNK_LOGW("csweb", "no media backend (Starfish: %s; NDL: %s)", why_sf.c_str(), why_ndl.c_str());
    return std::make_unique<PacingBackend>();
}

bool is_adaptive(const runtime::Link &link) {
    return link.link_type == "M3U8" || link.link_type == "DASH" || AdaptiveSource::handles(link.url);
}

std::unique_ptr<MediaSource> make_source(const runtime::Link &link, std::shared_ptr<wnk::net::HttpTransport> http,
                                         const DeviceMediaCaps &device) {
    wnk::net::Headers headers;
    for (auto &h : link_headers(link)) headers.push_back(std::move(h));
    if (is_adaptive(link)) {
        AdaptiveSourceOptions o;
        o.http = std::move(http);
        o.headers = std::move(headers);
        if (device.hevc == false) o.video_codecs = {CodecId::H264};
        if (device.uhd_panel != true) o.max_height = 1080;
        o.dolby_vision = device.dolby_vision == true;
        return std::make_unique<AdaptiveSource>(o);
    }
    FfmpegSourceOptions o;
    o.http = std::move(http);
    o.http_stream.headers = std::move(headers);
    return std::make_unique<FfmpegSource>(std::move(o));
}

}  // namespace csweb::player
