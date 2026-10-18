# Native App Kit audit

Inspected 2026-10-19. Evidence tags follow the kit's own legend (`docs/research/README.md` in the kit):
`[SRC]` source read, `[BUILD]` built here, `[TEST]` automated test here, `[HW]` run on our TV,
`[INF]` inference, `[UNK]` unknown.

## Revision

| Item | Value |
|---|---|
| Repository | https://github.com/Native-Webos/webos-native-app-kit |
| Commit | `e10f35ecd6e27987cc866b28caea6be66a023fcc` (2026-10-19T06:52:22+11:00) |
| Checkout | `external/webos-native-app-kit` |
| Licence | GPL-3.0-or-later (`LICENSE`, `LICENSING.md`, `THIRD-PARTY-NOTICES.md`) |

## What the kit is `[SRC]`

A C++17 / SDL2 (bundled SDL-webOS 2.30.12) / OpenGL ES 2 foundation for **native** apps on retail LG
webOS TVs in Developer Mode (unrooted). It owns the process, window, GLES context, main loop, input,
focus, layout, shutdown, and provides: logging with redaction, platform probes and Luna calls,
`async` (WorkerPool, MainDispatcher, CancellationToken, Lifetime), `net` (libcurl dlopened at run time,
WebSocket, mDNS), `image`, `storage` (DB8 key/value, BlobStore, SecretStore), `i18n`, `audio`, `ui`
(widgets, CollectionView, TextField with LG keyboard, Navigator, dialogs), `app` shell (`wnk::app::run`
+ `Delegate`), `diagnostics`, `services/auth`, `media` (FFmpeg source, Starfish/NDL backends,
DASH/HLS, subtitles). Four reference apps: hello (A), tvui (B, default template), shows (C,
networked), player (D).

Documents read: `README.md`, `AGENTS.md`, `docs/AGENT-APP-GUIDE.md`, `LICENSING.md`,
`CMakePresets.json`, `CMakeLists.txt`, `cmake/WebOSPackage.cmake`, `tools/build-webos.sh`,
`tools/check-package.py`, `tools/mkipk.py`, `tools/logs.sh`, `docs/research/README.md`,
`docs/research/PACKAGING-AND-FIRMWARE.md`, `docs/research/WEBOS-PLATFORM.md` (grep),
`docs/research/TOOLCHAIN-AND-BUILD.md` (ABI section), `docs/research/COMPATIBILITY.md` (grep),
`docs/compatibility/records.json`.

## Constraints that matter for a plugin runtime

| Constraint | Source | Consequence for this project |
|---|---|---|
| TV is Developer Mode, **not rooted**; device runs are build → package → `tools/install.sh` → `tools/launch.sh` | kit `AGENTS.md` `[SRC]` | No chroot, no binder/ashmem kernel modules, no container runtimes (Waydroid/Anbox impossible). A runtime must be shipped inside the ipk. |
| Userland is **32-bit ARMv7, EABI5, soft-float calling convention** (`arm-webos-linux-gnueabi`), kernel aarch64 | `records.json` device `lg-oled65c1ptb-6.5.3` "4.4.84… aarch64 (32-bit userland)" `[HW by kit]`; `TOOLCHAIN-AND-BUILD.md` `[BUILD]` | Off-the-shelf `armhf` (hard-float) or Android `arm64`/bionic binaries do not run as-is. |
| glibc 2.28 on the TV, 2.12.2 in the NDK sysroot | `COMPATIBILITY.md` `[BUILD]` | A runtime built elsewhere must need ≤ GLIBC_2.28 symbols (or the NDK's 2.12). |
| Only tmpfs `/tmp/<appid>` and `/dev/shm` writable; `/media/developer` EACCES, app dir read-only | `records.json` `storage.durable_filesystem_write` `[HW by kit]` | Runtime caches (dalvik-cache, oat files) can only live on tmpfs and are lost on reboot; persistent data must go to DB8. |
| `/dev/urandom` reportedly not exposed in the jail | `WEBOS-PLATFORM.md` line 77 (punktfunk claim) `[SRC]` | TLS/SecureRandom in a runtime needs `getrandom(2)`. Measured by our probe (see platform-capability-matrix.md). |
| No `CAP_SYS_NICE` | `WEBOS-PLATFORM.md` line 76 `[SRC]` | Runtime thread priorities cannot be raised. |
| Firmware-variable libraries are dlopened; **never bundle libstdc++**; every packaged ELF must be ELF32 ARM soft-float; no packaged ELF may need `libstdc++.so` | kit `AGENTS.md`, `tools/check-package.py` lines 143–160 `[SRC]` | ART is a set of C++ shared libraries. Bundling it needs either static libstdc++ per ELF or a reviewed, justified extension of the bundling policy for a separate child process (which does not load LG libraries, so the shadowing reason behind the rule does not apply there `[INF]`). Not to be bypassed silently. |
| Pinned `DT_NEEDED` for the main executable | `check-package.py` `[SRC]` | A runtime run as a child process does not change the app's DT_NEEDED. |
| One main thread owns SDL/GL/UI; blocking work on `async::WorkerPool`; results via `MainDispatcher::post` | `AGENT-APP-GUIDE.md` `[SRC]` | Runtime IPC must be driven from workers. |
| ipk is replaced wholesale on install | `PACKAGING-AND-FIRMWARE.md` A.2 `[SRC]` | Plugins cannot be stored in the app dir; downloaded `.cs3` files can live on tmpfs (volatile) or as DB8 blobs (`BlobStore`, 8 MiB per value default). |

## Kit facilities the CloudStream app would reuse `[SRC]`

| Need | Kit facility |
|---|---|
| Screens, grids, search box, details, dialogs | `ui/` (`Navigator`, `CollectionView`, `Card`, `TextField`, `ScrollView`, `Dialog`) |
| HTTP for repository JSON and plugin download | `net::HttpClient` (libcurl from the TV, cancellation, limits) |
| Persistent settings, repository list, installed plugin metadata, plugin blobs | `storage` DB8 store, `BlobStore` |
| Background work, cancellation | `async::WorkerPool`, `CancellationToken`, `run_then_post` |
| Playback of extracted links (HLS/DASH/MP4, headers) | `media/` `AdaptiveSource`, `FfmpegSource`, `PlaybackController`, Starfish/NDL |
| Diagnostics | `diagnostics/` capabilities, crash record, `tools/logs.sh` |

The kit has **no** facility for running Java/Kotlin/DEX code, no JVM, no Android runtime, and no
process-supervision helper for a second executable `[SRC]` (grep of the tree for `exec`, `posix_spawn`,
`dalvik`, `jvm` finds none outside crash handling).

## Integration choice for project apps

The kit guide puts apps in `applications/<name>/` with an `add_subdirectory` line in the root
`CMakeLists.txt`. To keep project code outside the kit checkout, we added one documented hook,
`WNK_EXTRA_APP_DIRS` (see `docs/kit-patches/0001-extra-app-dirs.md`). It changes no kit behaviour
when unset; the kit's own test and build scripts are used unchanged.
