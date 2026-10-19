# Agent Status Report

Date/time: 2026-10-19
Branch/commit: `main` (project repository; see `git log`)
Native App Kit commit: `e10f35ecd6e27987cc866b28caea6be66a023fcc` + `docs/kit-patches/0001` to `0006`
CloudStream: source `eb962b4fb7b1387b8de42ec58000cee0a01fe741`; pre-release APK sha256 `add4b760…6d83`
Runtime: Alpine art_standalone 0_git20251009-r3 (aarch64 on the TV, x86_64 on the host), wolfSSL 5.9.4
rebuilt with `WOLFSSL_ASN_ALLOW_0_SERIAL` (docs/runtime/tls.md)
Target TV / firmware: LG OLED65C1PTB, webOS 6.5.3 (kisscurl-koli), firmware 03.53.45, Developer Mode
Toolchain / NDK: arm-webos-linux-gnueabi gcc 12.2.0; OpenJDK 17 + D8 8.5.35 and clang/lld in alpine:edge containers

## Current phase and gate
Phase: 5/6 (TV UI and playback) in progress; the app `apps/csweb-tv` (org.csweb.tv 0.1.0) runs on the host
and on the TV.
Runtime feasibility: **PASS** (scoped; see docs/FEASIBILITY_DECISION.md)

## Completed work
- Phases 0-1: audits, TV probe, runtime evaluation, Android compat layer + csloader, plugin resources and
  settings dialogs, relocatable runtime, on-TV plugin execution, long-lived runtime service, AOT runtime.
- CSWeb TV app, mirroring CloudStream's own flows (MainActivity, HomeFragment, SearchFragment,
  ResultFragment, GeneratorPlayer, LibraryFragment, ExtensionsFragment, AccountSelectActivity):
  - Extensions: repositories (URL, cloudstreamrepo://, shortcodes), plugin install with hash checks,
    update, remove, plugin settings dialogs; plugin data kept across reboots (DB8 snapshot).
  - Home: one chosen source (None/Random/provider) with its main page, Continue watching.
  - Search: all allowed providers in parallel, grouped results, history, TvType chips.
  - Details: metadata, season/dub groups, episode ranges, bookmarks (WatchType), resume, cast row,
    recommendations.
  - Player: loadLinks with Skip loading, CloudStream link priorities, next mirror on error, sources,
    subtitles (plugin files, stream tracks, auto-select language, sync offset, size/box style), audio,
    resume position, next episode (autoplay setting), Episodes overlay, seek times from Settings.
  - Library: WatchType tabs, sort orders of the local list.
  - Settings: provider languages, preferred media, Test extensions (CloudStream's provider test),
    preferred quality, autoplay, seek times, subtitle language/size/style, show cast, clear history,
    profiles; app and runtime state.
  - Profiles: local profiles with their own library, home source, chips and history; optional 4-digit
    PIN (stored as a digest); "Who's watching?" picker at start (design 13: round avatars, colours,
    PIN pad).
  - Start: CloudStream's splash (the cloud on the app background); first-run setup (welcome,
    "Who's watching?" profiles, then Extensions or Home) until HAS_DONE_SETUP_KEY is set.
  - Look: CloudStream's cloud as the launcher icon with a "webOS" label (tools/make_brand.py);
    frosted glass over artwork (blurred backdrop composed per panel, glass.hpp) for the top bar and
    the buttons over posters. Host only so far; not yet seen on the TV.
  - Deep links: cloudstreamrepo://, https://cs.repo?…, cloudstreamsearch://.
- Kit patch 0003: every URL query value redacted in logs (stream tokens).
- Kit patch 0004: long labels (YouTube descriptions) no longer stall the UI (found on the TV with the
  app's stall sampler, `/tmp/<app>/stalls.log`).
- Kit patch 0005: DASH with several Periods; WebM DASH (VP9, Opus) indexed by its Cues.
- Kit patch 0006: the player's HTTP transport keeps cookies (Set-Cookie on a playlist, checked on
  segments), as CloudStream's Cronet source does; link headers reach every request.
- YouTube: NewPipe set up as in CloudStream; separate audio tracks merged (MergedSource); plays on
  the TV (4K VP9, no dropped frames).
- ClearKey DRM links (DrmExtractorLink) for DASH and MP4; Widevine/PlayReady listed as unsupported.
- Subtitle headers (SubtitleFile.headers) sent as CloudStream does.
- `csweb-media-probe`: host tool, a URL through the player's source.
- Conformance fixture v2 (own test plugin): adds a two-season series with actors.

## Commands executed (latest session)
| Command | Exit | Notes |
|---|---:|---|
| `cmake --build --preset host-debug -j12` | 0 | no warnings |
| `ctest --preset host-debug` (kit) | 0 | 106/106, incl. 17 csweb suites |
| `docker run … eclipse-temurin:17-jdk sh test-plugins/conformance/build.sh` | 0 | fixture v2 sha256 b964798d… |
| `cmake --build --preset webos-release --target csweb-tv-ipk` | 0 | 77.9 MB, check-package passed |
| `tools/install.sh … --private-runtime runtime-aarch64` | 1 | TV unreachable (powered off) |

## Tests
- Passed: kit host suite 106 (csweb: sha256, zip_check, repository, runtime_types, plugin_store,
  runtime_client, episode_groups, redact, deep_link, library, dir_snapshot, preferences, provider_test,
  profiles, link_priority, subtitle_file); compat 165; scripted host UI runs (settings, chips, picker,
  subtitle sync, provider tests, profiles and PIN, library sort, series/episodes/cast).
- Provider test on the host: Internet Archive passes; Dailymotion fails inside CloudStream's bundled
  Dailymotion extractor (the API now sends `subtitles.data` as an array); Invidious' public instance fails.
- Not yet on the TV: this session's features (the TV was off after the 0.1 first-launch run).

## Evidence
- TV: `evidence/device/20261019-csweb-tv-0.1-first/` (AOT runtime ready ~0.5 s, first frame ~1 s,
  repository deep link), earlier probe runs under `evidence/device/`.
- Host app: `evidence/app-host/20261019-csweb-tv-0.1-stage2/`; TLS: `evidence/runtime-host/tls/`.

## Risks/blockers
- Features not built, by platform limits or scope: downloads (no writable storage; removed at the
  user's request), AniList/MAL/Simkl sync (OAuth needs a browser), trailers (YouTube), AniSkip,
  OpenSubtitles search, playback speed (the kit's PlaybackController has no rate control), NSFW toggle
  (the runtime keeps CloudStream's default, adult off), pinned providers, backup/restore.
- Plugins needing a WebView, custom/AndroidX views or fragments are unsupported.
- MainAPI.getVideoInterceptor (OkHttp interceptors on video requests) is not applied: the player's
  requests are native. Hierarchical sidx DASH, TTML subtitles and Widevine/PlayReady do not play.
- Licence audit of the bundled runtime still metadata-only (needed before any redistribution).
- Package 78 MB; the AOT boot image is tied to the app id through /tmp/<app id>/rt.

## Next tasks
1. Install the current ipk on the TV; with the remote: install DailymotionProvider and
   InternetArchiveProvider, play an Internet Archive item (patched wolfSSL on the TV), try profiles,
   settings, Test extensions; collect logs into evidence/device/.
2. Phase 7: licence audit, security review, compatibility matrix, release report.
