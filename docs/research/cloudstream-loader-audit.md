# CloudStream plugin loader audit

Source: https://github.com/recloudstream/cloudstream commit `eb962b4fb7b1387b8de42ec58000cee0a01fe741`
(2026-10-18T19:45:11+02:00), inspected 2026-10-19. Paths below are relative to that checkout
(`external/cloudstream`). All claims `[SRC]` unless tagged.

## Pipeline

```
repository URL → repo.json (Repository: name, pluginLists[]) → plugins.json (SitePlugin[])
  → download <internalName>.cs3 to filesDir/online_plugins/<repo folder>/<file>.cs3
  → setReadOnly() (Android 14+ requires read-only dex)
  → PathClassLoader(file, context.classLoader)                     ← Android/ART-specific
  → getResourceAsStream("manifest.json") → BasePlugin.Manifest
  → loadClass(manifest.pluginClassName) → getDeclaredConstructor().newInstance()
  → [requiresResources] AssetManager.addAssetPath(file) + Resources(...)   ← Android framework
  → plugin.filename = path; register in plugins/classLoaders/urlPlugins maps
  → Plugin.load(context)  (Android Plugin)   or   BasePlugin.load()  (platform-neutral)
       → registerMainAPI(MainAPI) → APIHolder.allProviders + addPluginMapping
       → registerExtractorAPI(ExtractorApi) → extractorApis
  → provider operations: MainAPI.getMainPage / search / load / loadLinks (suspend functions)
  → unloadPlugin: beforeUnload(); remove APIs, extractors, VideoClickActions, loaders
```

## Key code

| Step | Location |
|---|---|
| Manifest class (`name`, `pluginClassName`, `requiresResources`, `version`) | `library/src/commonMain/kotlin/com/lagradost/cloudstream3/plugins/BasePlugin.kt` (`BasePlugin.Manifest`) |
| `registerMainAPI`, `registerExtractorAPI`, `load()`, `beforeUnload()`, `filename` | same file |
| Android `Plugin : BasePlugin` with `load(context: Context)`, `resources`, `openSettings`, `registerVideoClickAction` | `app/src/main/java/com/lagradost/cloudstream3/plugins/Plugin.kt` |
| `loadPlugin` (class loader, manifest, instantiation, resources, lifecycle) | `app/src/main/java/com/lagradost/cloudstream3/plugins/PluginManager.kt:593-687` |
| `unloadPlugin` | `PluginManager.kt:689-735` |
| Plugin path `filesDir/online_plugins/<folder>/<name>.cs3` | `PluginManager.kt:747-755` |
| Repository/SitePlugin JSON model, download | `app/src/main/java/com/lagradost/cloudstream3/plugins/RepositoryManager.kt` (`Repository` l.34, `SitePlugin` l.50) |
| `app` (NiceHttp `Requests`) used by providers | `library/src/commonMain/kotlin/com/lagradost/cloudstream3/MainActivity.kt` → JVM class `MainActivityKt.getApp()` |

## Android assumptions

1. **Class loading** uses `dalvik.system.PathClassLoader` on the `.cs3` file itself: the runtime must load
   DEX from a ZIP (ART does; a JVM does not) and expose `getResourceAsStream("manifest.json")` from it.
2. **Parent class loader** is the app's: plugin references to `com.lagradost.cloudstream3.*`, Kotlin,
   coroutines, Jackson, NiceHttp/OkHttp, Jsoup etc. resolve against **the app's copies**. A host must
   therefore provide these with binary-compatible signatures (checked statically below).
3. `Plugin.load(Context)` and `requiresResources` need `android.content.Context`, `AssetManager`,
   `Resources`. Plugins that extend `BasePlugin` and set `requiresResources=false` need none of these.
4. Errors are logged with `android.util.Log`; the library itself uses `com.lagradost.api.Log`
   (expect/actual; JVM actual exists).

## Platform-neutral surface `[SRC]`

The `library` module is Kotlin Multiplatform with an **Android target and a `jvm()` target**
(`library/build.gradle.kts`). `BasePlugin`, `MainAPI`, `APIHolder`, extractor APIs and `app` are in
`commonMain`/`jvmCommonMain`. The binary API of the JVM target is recorded in
`library/api/jvm/library.api` (Kotlin binary-compatibility validator). Upstream's `desktopApp`
(Compose) does **not** load plugins (no class-loader code in `desktopApp/`, `shared/`).

## Static ABI check of real plugins against the JVM API `[TEST]`

`tools/check_api_refs.py` resolves every `com/lagradost/cloudstream3/*` type, method and field a
plugin's DEX references against `library/api/jvm/library.api` (including inherited members).

| Plugin | References checked | Missing |
|---|---:|---:|
| InternetArchiveProvider v1 | 85 | **0** |
| DailymotionProvider v4 | 34 | **0** |
| InvidiousProvider v9 | 53 | **0** |

Output: `evidence/plugins/dexinfo/*.apicheck.json`. Unit tests with a negative control:
`tests/test_tools.py`. This proves **signature presence only**, not behaviour.

## Implication

For `BasePlugin` plugins the only Android-specific requirement is **executing DEX through a
`PathClassLoader`-compatible class loader**. Everything else is available from upstream's JVM
library artifacts, unchanged. `Plugin` (Context-based) plugins and `requiresResources` plugins need an
Android framework subset; they are out of scope until a BasePlugin provider passes the gate.
