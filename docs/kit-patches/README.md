# Kit patches

Changes this project makes to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc`, applied in
order to `external/webos-native-app-kit`. Each has a note (`*.md`) and the diff (`*.patch`).

All six were offered upstream on 2026-10-20 as separate pull requests, each from the kit's `main`
with extra tests and guide updates (one commit per change, checked with `tools/test.sh` host-debug
and host-asan; 0006 also under ThreadSanitizer):

| Patch | Upstream pull request |
|---|---|
| 0001 extra app dirs | https://github.com/Native-Webos/webos-native-app-kit/pull/1 |
| 0002 private runtimes | https://github.com/Native-Webos/webos-native-app-kit/pull/2 |
| 0003 redact URL queries | https://github.com/Native-Webos/webos-native-app-kit/pull/3 |
| 0004 label ellipsis cost | https://github.com/Native-Webos/webos-native-app-kit/pull/4 |
| 0005 DASH periods, WebM | https://github.com/Native-Webos/webos-native-app-kit/pull/5 |
| 0006 HTTP cookies | https://github.com/Native-Webos/webos-native-app-kit/pull/6 |

The upstream versions differ slightly from the patches here: 0001 moved into `cmake/ExtraApps.cmake`
with path checks, 0002's CMake comment is more precise, and every one has more tests. Once they are
merged, the project can move to the kit commit that has them and drop the patches.
