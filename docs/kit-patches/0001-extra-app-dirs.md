# Kit patch 0001: `WNK_EXTRA_APP_DIRS`

> Merged upstream as https://github.com/Native-Webos/webos-native-app-kit/pull/1 (merge `bb22abc`, 2026-10-19); since kit `5f91c56` the project no longer carries this patch. Kept as the record of why and how it was tested; the patch file is in git history.

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc`. Patch: `0001-extra-app-dirs.patch`.

## What
Adds a cache variable `WNK_EXTRA_APP_DIRS` (initialised from the environment variable of the same name)
to the kit's root `CMakeLists.txt`. Each listed directory is added with `add_subdirectory` after the
kit's examples and tests, so it can call `wnk_add_webos_package()` and register `add_test()` tests.

## Why
`docs/AGENT-APP-GUIDE.md` asks for product apps in `applications/<name>/` plus an `add_subdirectory`
line. This project keeps its own sources (`apps/`) outside the upstream checkout so the kit can be
updated by `git pull` and the project's changes stay reviewable. The hook is the smallest change that
lets the kit's unmodified `tools/test.sh`, `tools/build-webos.sh`, packaging and checks build our apps.

## Effect when unset
None: the block is skipped.

## Usage
```sh
export WNK_EXTRA_APP_DIRS=$PWD/apps/runtime-probe
cd external/webos-native-app-kit && tools/test.sh && tools/build-webos.sh
```
The value is cached on first configure; delete `build/<preset>/CMakeCache.txt` after changing it.
