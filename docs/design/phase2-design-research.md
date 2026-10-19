# Phase 2 design research

Status 2026-10-19: research done, screen designs drafted for approval. No app code yet.
Designs: canvas "CloudStream webOS — screen designs" (claude.ai artifact, private to the owner).

## Sources studied

| App | What was read | Version |
|---|---|---|
| PlxNative (Plex client for webOS, GPU-rendered, no browser) | `~/Desktop/native-jelly` (its Jellyfin port): `docs/DESIGN.md`, `docs/screenshots/*` | local checkout |
| Nuvio TV (Android TV, Kotlin + Compose TV Material 3) | `external/nuviotv` shallow clone: `ui/screens/*`, `res/values/strings.xml` | `719f2b7a` |
| CloudStream | `external/cloudstream` `ui/*`, `res/xml/settings_*.xml`, `syncproviders/` | `eb962b4` |

## What each contributes

**PlxNative** (look and TV ergonomics)
- One fixed 1920×1080 surface; focus is a first-class state (no hover), remote-only input.
- Full-bleed artwork with a graded scrim so text stays legible; no per-label chips over art.
- Floating glass navigation pill at the top; profile avatar left; search as an icon in the pill.
- Type scale with a couch-distance floor (24 px for content, 72 px hero); white pill primary button
  plus round icon buttons; shelves of posters/landscape stills with a focus "pop" (scale + ring).
- Minimal player: title, scrubber, a few round controls; Info/Chapters chips.

**Nuvio TV** (structure and behaviour)
- Three home layouts (Classic rows, Grid, Modern: fixed hero + one active row), landscape/poster
  card choice, optional collapsible floating sidebar.
- Stream screen as a dedicated picker with auto-play rules: first stream, smart match, manual.
- Player: episodes side panel, next-episode end prompt, pause overlay with info, audio/subtitle
  overlays, skip intro/outro from segment databases.
- Essential / Advanced settings modes; ambient mode (artwork when idle); QR/device-code account linking.

**CloudStream** (the feature set to cover)
- Home per provider main page with a provider switcher; continue watching; bookmarks.
- Search across all providers, type filters, history, quick search.
- Details: metadata, tags, rating, cast, trailer, recommendations, seasons, dub/sub, episode
  ranges for long anime, fillers, bookmark status (Watching, Plan to watch, On hold, Completed,
  Dropped), subscribe to new episodes, download.
- Links: multiple extractors/qualities, subtitles per link, source priority.
- Player: quality/source switch, subtitles (embedded, from source, OpenSubtitles/Subdl search),
  subtitle sync and style, speed, resize, skip OP/ED (AniSkip), next episode.
- Library (local and AniList/MAL/Kitsu/Simkl lists), downloads with queue.
- Extensions: repositories, plugin install/update/remove, per-plugin settings, provider tests.
- Accounts with PIN, setup wizard (language, content languages, media types, layout), settings
  (general, player, subtitles, providers incl. NSFW off by default, UI, updates and backup).

## Screens drafted (16)

Home · Search · Details (movie) · Details (series and episodes) · Source picker · Player controls ·
Player tracks (subtitles/audio/quality/speed) · Up next, skip intro, episodes panel · Library ·
Downloads · Extensions · Plugin settings (rendered from the plugin through the dialog protocol) ·
Profiles (PIN) · First-run setup · Settings · Accounts and sync (QR sign-in).

## Decisions to confirm with the user

- Primary navigation: top glass pill (PlxNative) vs collapsible sidebar (Nuvio option).
- Home layout default: hero + rows (drafted) vs Nuvio's Modern single-active-row.
- Downloads need writable storage: in Developer Mode only tmpfs was found writable; a USB drive is
  assumed in the design and must be verified on the TV.
- Plugins needing a WebView are shown as "Not supported" rather than hidden.
- Accent colour, typeface (TV system font on device; Figtree in the mockups).
