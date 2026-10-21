# Legacy package: TVs with a 32-bit kernel

The main package (`org.csweb.tv`) needs two things some older LG TVs do not have:

| Needs | Why | Seen on an old TV (webOS 4.4.3, Mali-G51) |
|---|---|---|
| a 64-bit kernel | the plugin runtime is `runtime-aarch64` (ART, musl, aarch64) | `kernel … armv7l` in the log, then `runtime: exited (code 127, signal 0)` four times: `execve` of an aarch64 ELF fails |
| `libcurl.so.4` in the firmware | the kit dlopens it for all HTTP (webOS 5.3.1+ has it) | `no HTTP transport: libcurl.so.4: cannot open shared object file`, `net.http unsupported` |

`armv7l` from `uname` means a 32-bit kernel. (A 64-bit kernel reports `aarch64`, or `armv8l` to a 32-bit
process.) Such a TV cannot start any 64-bit program, so a 64→32 bridge such as webos-bridge-64to32 does not
help either.

The legacy package, `org.csweb.tv.legacy`, is the same app built with `-DCSWEB_LEGACY=ON`:

- `runtime-armv7/`: the plugin runtime from Alpine's **armv7** packages (`art_standalone` is built for
  armv7 in Alpine edge/testing), started through `ld-musl-armhf.so.1`;
- `lib/libcurl.so.4`: curl + mbedTLS built with the webOS NDK (`tools/build-libcurl-webos.sh`), verifying
  against `runtime-armv7/trust/ca-bundle.pem` (the same Mozilla list as the runtime's Java trust store);
- its own app id, so it installs beside the main package and a Homebrew Channel update of one never
  replaces the other.

## Nothing changes for the main package

Every change is opt-in. Without the new options each script and the app build exactly as before:

| Piece | Default (main package) | Legacy |
|---|---|---|
| `tools/build-runtime.sh` | `--arch aarch64` (default): unchanged steps | `--arch armv7` |
| `tools/fetch-runtime-inputs.sh` | `runtime/inputs.lock` | also `runtime/inputs-armv7.lock` |
| `runtime/native/build-native.sh`, `build-wolfssl.sh` | aarch64 + x86_64 | `armv7` argument |
| `runtime/make-aot.sh`, `make_dist.py` | aarch64 / x86_64 cases untouched | new `armv7` case |
| `runtime/tools/notices.py`, `collect-licenses.sh` | `runtime/licenses/` | `CSWEB_LICENSES_DIR=…/licenses-armv7` |
| app sources | the same preprocessed code | `#ifdef CSWEB_LEGACY` (app id, runtime folder, loader name) |
| `apps/csweb-tv/CMakeLists.txt` | `CSWEB_LEGACY=OFF` | `org.csweb.tv.legacy`, `runtime-armv7`, bundled libcurl |
| CI | `host`, `runtime`, `webos` unchanged | new `runtime-legacy`, `webos-legacy` jobs, `continue-on-error` |
| release | the same files | legacy ipk + manifest added only when built **and** `runtime/licenses-armv7/` exists |

## Building it

```
tools/build-runtime.sh --arch armv7          # runtime/dist/runtime-armv7 (docker)
cd external/webos-native-app-kit
cmake --preset webos-release -B build/webos-legacy \
  -DWNK_EXTRA_APP_DIRS=$PWD/../../apps/csweb-tv -DCSWEB_LEGACY=ON
cmake --build build/webos-legacy --target csweb-tv-ipk
# -> build/webos-legacy/dist/org.csweb.tv.legacy_<version>_arm.ipk
```

Configure runs `tools/build-libcurl-webos.sh` once (cached by its inputs), like the kit's FFmpeg.

## First-time setup (once per move of the pins)

Until `runtime/inputs-armv7.lock` exists, `build-runtime.sh --arch armv7` takes the armv7 packages unpinned
from Alpine edge (CloudStream stays pinned) and only reports missing licence notices. To pin and license it,
after a build that worked on a TV:

```
python3 runtime/tools/make_lock.py armv7          # writes runtime/inputs-armv7.lock from rootfs-armv7.manifest
#  upload ~/.cache/csweb-runtime-inputs/alpine/armv7/*.apk to a runtime-inputs-N release (asset name =
#  path with / -> _) and add its "# mirror: <url>" line to the lock
export CSWEB_LICENSES_DIR=$PWD/runtime/licenses-armv7
python3 runtime/tools/notices.py packages runtime/rootfs/armv7 runtime/dist/runtime-armv7
docker run --rm -v $PWD:/p -e CSWEB_LICENSES_DIR=/p/runtime/licenses-armv7 alpine:edge sh /p/runtime/collect-licenses.sh
python3 runtime/tools/notices.py render
```

Commit `runtime/rootfs-armv7.manifest`, `runtime/inputs-armv7.lock` and `runtime/licenses-armv7/`. From then
on the armv7 build is pinned, its notices check is strict, and releases include the legacy package.

## To check on the TV

The legacy package is built by CI but, like the main one, not run on a TV by CI. Things to watch in its log
(`/tmp/org.csweb.tv.legacy/`):

- `runtime: launching` followed by the runtime becoming ready, not `exited (code 127)`;
- `net.http supported libcurl/8.23.0 mbedTLS/3.6.7`; adding a repository then works;
- memory: a 32-bit process has about 3 GB of address space and old TVs have little free RAM. The heap is
  `-Xmx256m` (`runtime_launcher.hpp`); lower it if ART cannot reserve it;
- AOT: `runtime/make-aot.sh` compiles for a conservative ARMv7 feature set. If the build could not AOT-compile,
  the log says `AOT: no AOT.json` and ART starts without a boot image (slower, still works). Once dalvikvm has
  run on the TV, put the `--instruction-set-features` it logs into `make-aot.sh`.
- The libcurl's CA bundle path is compiled in as
  `/media/developer/apps/usr/palm/applications/org.csweb.tv.legacy/runtime-armv7/trust/ca-bundle.pem`
  (where both developer mode and the Homebrew Channel install apps; `CSWEB_LEGACY_INSTALL_DIR` changes it).
  A TLS failure naming that file means the app is installed elsewhere.
