# CSWeb TV 0.1.0, stage 2 on the host

2026-10-19, host x86_64 (Ubuntu 24.04), kit host-debug build, plugin runtime `runtime/dist/runtime-x86_64`
(imageless ART), Xvfb 1280×720, scripted keys (`WNK_CSWEB_KEYS`). Pacing backend: no picture or sound on
the host; the source side (network, demux) is the TV's.

Plugins, all unmodified, installed through the app: Dailymotion v4, InternetArchive v1, Invidious v9
from the official repository `https://raw.githubusercontent.com/recloudstream/extensions/master/repo.json`;
the project's own `CswebConformance.cs3` (sha256 `579d0d25…8632`) from a local test repository
(`http://127.0.0.1:8765/repo.json`, plugin list with `fileHash`).

| Shot | What it shows |
|---|---|
| `d_repo.png` | official repository's plugin list: installed versions, install buttons |
| `m_settings.png` | the conformance plugin's own settings layout, described by the runtime, drawn natively |
| `e_home.png` | Home from Dailymotion's main page: hero with loaded details, "Popular" row |
| `n_search.png` | one query to every provider in parallel; results grouped by provider |
| `i_details.png` | a Dailymotion item's details |
| `p_play.png` | the conformance link (archive.org MP3, LibriVox) playing: 0:25 / 18:52 after a +10 s seek |

`p_play.log` (queries redacted): links → FFmpeg source opened `mp3, 1133.0 s` → Playing → Seeking →
Playing; frames at 16 ms with the player on screen.

Not working here, not attributed to the app: Dailymotion's HLS manifest answers 403 (`x-error-code: E005`)
to any client tried (curl with CloudStream's user agent too); Internet Archive and Invidious searches did
not answer (Internet Archive: the runtime's known archive.org TLS problem; Invidious: public instance).
