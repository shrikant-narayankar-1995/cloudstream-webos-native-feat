# Plugin format audit

Inspected 2026-10-19 **statically only**. No plugin code has been executed anywhere.

## Source

| Item | Value |
|---|---|
| Repository | https://github.com/recloudstream/extensions (official CloudStream organisation) |
| Plugin list | `https://raw.githubusercontent.com/recloudstream/extensions/builds/plugins.json` |
| `builds` commit | `fcb6101ce646951f5055a6aff67ee2e9f3e1fd54` ("Build cf16b3b…", 2026-05-22) |
| Source commit | `master` `cf16b3b45cda911a36ddca69f9acbe4763c6a4b1` |
| Licence | **none declared** by GitHub for this repository `[UNK]`; CloudStream itself is GPL-3.0. Redistributing these plugins is not planned; they are downloaded by the user at run time. |
| Fetched | `evidence/plugins/download/FETCHED_AT`; hashes in `evidence/plugins/download/SHA256SUMS` |

`plugins.json` lists five providers (all `status: 1`, `apiVersion: 1`): Dailymotion, InternetArchive,
Invidious, Twitch, Youtube.

## Selected test plugins

| Plugin | SHA-256 | Manifest | Why |
|---|---|---|---|
| **InternetArchiveProvider.cs3** (primary) | `54f3de560bebeff2e4a10d1163f6146313c59061d21c3a230125611f01b531eb` | `{"requiresResources":false,"version":1,"pluginClassName":"recloudstream.InternetArchivePlugin","name":"InternetArchiveProvider"}` | Public-domain / openly licensed content; official repo; small |
| **DailymotionProvider.cs3** (secondary) | `9036525a64e8b3c8fe04f94e9fe89a744c13d69af684ed0d2ad2a7eb0f332cd9` | `{"requiresResources":false,"version":4,"pluginClassName":"recloudstream.DailymotionPlugin","name":"DailymotionProvider"}` | Public platform API; its host's TLS chain works with the candidate runtime (see dependency-inventory.md) |
| InvidiousProvider.cs3 (reference) | `a0a2ff7e8484f69118598d347458774935733399ba0f76ef0d4921886a7db4a9` | `…"pluginClassName":"recloudstream.InvidiousPlugin"…` | Static comparison only |

## Archive structure (`tools/dexinfo.py`, ZIP listing)

| Plugin | Entries | DEX |
|---|---|---|
| InternetArchive | `manifest.json` (128 B), `classes.dex` (67 456 B) | `dex\n035\0`, 33 classes, 366 method refs, 773 strings |
| Dailymotion | `manifest.json` (120 B), `classes.dex` (26 036 B) | `035`, 14 classes |
| Invidious | `manifest.json` (116 B), `classes.dex` (30 568 B) | `035`, 16 classes |

No native libraries (`lib/*.so`), no resources (`resources.arsc`), no assets, no nested archives, no
symlink or traversal entries, regular file modes `0644`.

## Entry class and provider (InternetArchive)

- `recloudstream.InternetArchivePlugin extends com.lagradost.cloudstream3.plugins.BasePlugin`
  (**not** the Android `Plugin` class).
- `recloudstream.InternetArchiveProvider extends com.lagradost.cloudstream3.MainAPI`, with coroutine
  continuation classes for `getMainPage`, `search`, `load`, `loadLinks`.

## External references (InternetArchive; full lists in `evidence/plugins/dexinfo/*.json`)

| Package group | Examples |
|---|---|
| CloudStream API | `MainAPI`, `MainAPIKt.newMovieSearchResponse/newMovieLoadResponse/newTvSeriesLoadResponse/newEpisode/newHomePageResponse`, `MainActivityKt.getApp()`, `ExtractorApiKt.loadExtractor/newExtractorLink`, `AppUtils.toJson`, `StringUtils.encodeUri`, `ArchComponentExtKt.logError` |
| HTTP | `com.lagradost.nicehttp.Requests.get$default`, `NiceResponse.getText`, `okhttp3.Interceptor` (type only) |
| JSON | Jackson `ObjectMapper`, `TypeReference`, `jackson-module-kotlin` `jacksonObjectMapper()` |
| HTML | `org.jsoup.Jsoup.parse`, `Document.select`, `Element.html/unwrap` |
| Kotlin | stdlib, `kotlin.coroutines.*`, `kotlin.jvm.internal.*`, `kotlin.text`, `kotlin.sequences` |
| Java | `java.lang`, `java.util`, `java.util.regex`, `java.util.concurrent` |
| Android | **none** (`android.*` count 0). `dalvik.annotation.*` appear only as DEX metadata annotations (Signature, InnerClass, EnclosingMethod, MemberClasses, SourceDebugExtension) |

Dailymotion and Invidious have the same shape (BasePlugin, no `android.*`).

## Safety notes
- Plugins are executable code with the runtime's full permissions. Even official-repo plugins contact
  third-party hosts. Hash agreement only proves the bytes match what we inspected.
- Execution is gated on explicit approval (`docs/research/runtime-test-plan.md`).
