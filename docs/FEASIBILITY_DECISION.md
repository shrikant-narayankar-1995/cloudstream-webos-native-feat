# Feasibility decision

**Status: PASS**

Date: 2026-10-19. Target: LG OLED65C1PTB, webOS 6.5.3 (kisscurl-koli), firmware 03.53.45, Developer Mode
(unrooted). Evidence log:
`evidence/device/20261019-075624-runtimeprobe-0.2/org.csweb.runtimeprobe-events-20261019-075836.log`.

## What passed on the TV

The native app `org.csweb.runtimeprobe` 0.2.0 (ipk sha256
`58bc621b2819352179494ab7c898688ef58155b262a6a3f49dc773fbb2e92097`, built and checked with the kit's own
tools) started the bundled Android runtime (Alpine `art_standalone` aarch64, ART with Android 10 libcore)
as its child process on the TV and, for each test, loaded an **unmodified** `.cs3`, ran its original
entry class and lifecycle, registered its original provider and ran a real provider operation:

| Plugin (SHA-256 checked before and after) | Entry class / base | Operation | Result on the TV | Wall | Peak RSS |
|---|---|---|---|---:|---:|
| DailymotionProvider v4 `9036525a…2cd9` (official repo) | `recloudstream.DailymotionPlugin` / BasePlugin | search "nasa" | 26 real results | 9.8 s | 117 MiB |
| LibriVoxAudiobook v34 `71e7aac0…a54b` (CNCVerse repo) | `com.RowdyAvocado.LibriVoxAudiobookPlugin` / Android `Plugin` + Context | search "Pride and Prejudice" | 2 results | 10.1 s | 118 MiB |
| LibriVoxAudiobook | same | load (details) | 37 chapters | 9.8 s | 115 MiB |
| LibriVoxAudiobook | same | loadLinks | 1 playable MP3 ExtractorLink with referer | 7.6 s | 111 MiB |
| InternetArchiveProvider v1 `54f3de56…31eb` (official repo) | `recloudstream.InternetArchivePlugin` / BasePlugin | search | **failed**: TLS handshake (wolfSSL ASN parse error on archive.org's chain) | 9.0 s | 114 MiB |

Every gate item of `.claude/docs/FEASIBILITY_GATE.md`:

| # | Requirement | Evidence |
|---|---|---|
| 1 | Runtime available and packageable | Alpine `art_standalone` 0_git20251009-r3 (Apache-2.0), installed with apk signature checks, pruned to 50 MiB, packaged as a kit "private runtime" (docs/kit-patches/0002) |
| 2 | Original DEX loads and executes | `PathClassLoader` on the .cs3 itself; SHA-256 unchanged |
| 3 | Original entry class resolves | `plugin` events above |
| 4 | CloudStream API classes, compatible signatures | the unmodified CloudStream pre-release APK DEX (built from `eb962b4`) is the parent class path |
| 5 | Android framework calls work | `runtime/android-compat` (Context, SharedPreferences, Base64, Uri, Log, Build, Resources, TLS helpers…), 110 host tests |
| 6 | Kotlin and third-party dependencies resolve | from the same APK (Kotlin, coroutines, OkHttp/NiceHttp, Jackson, Jsoup) |
| 7 | Constructor and lifecycle run | `load()` / `load(Context)` completed; providers registered |
| 8 | Provider registers | `provider` events (Dailymotion, Librivox Audiobook) |
| 9 | Real operation returns actual results | table above |
| 10 | All execution on the TV | child of the app (fork/exec), measured by the app with wait4; no remote service exists in the design |
| 11 | Resource and security constraints documented | below and platform-capability-matrix.md |

## Scope of the claim

Compatible **in principle and demonstrated** for plugins that (a) extend `BasePlugin`, or extend `Plugin`
and keep their UI behind an `is Activity` check, (b) do not require Android resources, and (c) do not reach
WebView. Host load-only pass: 31 of 40 surveyed plugins load and register (docs/research/plugin-compatibility.md).
Not supported: plugins that cast the context to `AppCompatActivity` in `load()` (8 of 40), `requiresResources`,
WebView-based extraction, plugin settings screens.

## Known defects and limits
- TLS: archive.org's certificate chain fails in the runtime's wolfSSL provider (InternetArchive, LibriVox
  *covers* and similar). To fix in the runtime layer (BouncyCastle JSSE or wolfSSL), not in plugins.
- Upstream CloudStream bug: Dailymotion extractor fails on `subtitles.data: []`.
- Each operation currently starts a fresh runtime: ~6–7 s of the ~10 s is start-up (imageless,
  verify-only). Next: one long-lived runtime process per session and a precompiled boot image.
- Memory: ~120 MiB peak per runtime process with ~436 MB available beside a small app.
- Isolation: plugins run with the app's uid and network access, in a separate process; no further sandbox.
- Licensing of the bundled runtime (wolfSSL GPL-3.0-only, OpenJDK libcore GPL-2.0+CE, ICU, musl…) is
  audited from metadata only; full audit before any redistribution.
