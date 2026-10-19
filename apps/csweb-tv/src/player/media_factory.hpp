// SPDX-License-Identifier: GPL-3.0-or-later
//
// The kit's media pipeline for one plugin link: the TV's decoder (Starfish with NDL as fallback,
// as the kit's player example measured; a pacing stand-in on the host) and the source for the
// link — DASH/HLS through the AdaptiveSource (by link type, or by extension), everything else
// through FFmpeg — both sending the link's headers.
#pragma once

#include "runtime/runtime_types.hpp"

#include "wnk/media/device_caps.hpp"
#include "wnk/media/media.hpp"
#include "wnk/net/http.hpp"

#include <memory>

namespace csweb::player {

std::unique_ptr<wnk::media::MediaBackend> make_backend(const wnk::media::DeviceMediaCaps &device);
std::unique_ptr<wnk::media::MediaSource> make_source(const runtime::Link &link, std::shared_ptr<wnk::net::HttpTransport> http,
                                                     const wnk::media::DeviceMediaCaps &device);
bool is_adaptive(const runtime::Link &link);

}  // namespace csweb::player
