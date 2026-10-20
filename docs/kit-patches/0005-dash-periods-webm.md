# Kit patch 0005: DASH with several Periods, WebM DASH

> Merged upstream as https://github.com/Native-Webos/webos-native-app-kit/pull/5 (merge `febd6fa`, 2026-10-19); since kit `5f91c56` the project no longer carries this patch. Kept as the record of why and how it was tested; the patch file is in git history.

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc` after patches 0001-0004.
Patch: `0005-dash-periods-webm.patch` (media/include/wnk/media/manifest.hpp, adaptive_source.hpp,
media/src/manifest.cpp, adaptive_source.cpp, tests/unit/test_manifest.cpp,
tests/integration/test_adaptive_source.cpp).

## What
- **Periods.** `parse_mpd()` reads every Period of a static MPD (a dynamic MPD keeps its first). Period
  starts and durations follow ISO/IEC 23009-1 (an absent @start is the previous start + duration).
  Each rendition of a later Period continues one of the previous Period (`Rendition::next_period`:
  same AdaptationSet and Representation ids, else same type, language and codec at the closest
  bandwidth); only first-Period renditions become tracks. `AdaptiveSource` opens the continuation
  when a lane's Period ends (own initialisation segment and presentationTimeOffset), clips packets to
  each Period's interval as ExoPlayer does (segments may overhang the boundary), and a seek starts in
  the Period holding the target. Bitrate switching and subtitles follow the first Period.
- **WebM.** WebM renditions (VP9, Opus) are no longer left out. With SegmentBase their index is the
  file's Cues: new `parse_webm_cues()` reads the Segment data offset and TimecodeScale from the
  Initialization range and gives one segment per Cluster.

## Why
CloudStream plays DASH through ExoPlayer, which supports both. Measured with the app's
`csweb-media-probe` (host): the DASH-IF multi-period test vector (`dash264/TestCases/5a/nomor/1.mpd`)
stopped at 250 s of 594 s; Shaka's `sintel-webm-only` failed with "no video rendition this device can
play". After: both play, seek and cross the Period boundary without lost or repeated audio.

Not changed: hierarchical sidx (FFmpeg's MP4 demuxer refuses `sidx` reference_type 1; DASH-IF
`1a/sony` case), TTML subtitles, audio bitrate switching.

## Tests
Unit: "dash: several Periods of a static MPD continue each other; a dynamic MPD keeps the first",
"webm: Cues give one segment per Cluster, timed by the TimecodeScale". Integration (ffmpeg CLI and
the test HTTP server): "DASH with two Periods plays through the boundary and seeks into the second",
"WebM DASH (VP9, Opus) indexed by its Cues". Whole host suite: 107/107.
