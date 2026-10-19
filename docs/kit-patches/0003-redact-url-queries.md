# Kit patch 0003: redact every URL query value in logs

> Merged upstream as https://github.com/Native-Webos/webos-native-app-kit/pull/3 (merge `68cc31f`, 2026-10-19); the project now uses kit `5f91c56` and no longer carries this patch. Kept as the record of why and how it was tested; the patch file is in git history.

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc` after patches 0001 and 0002.
Patch: `0003-redact-url-queries.patch` (logging/src/log.cpp, logging/include/wnk/log.hpp,
tests/unit/test_log.cpp).

## What
`wnk::log::redact()`, which every log message passes through before any sink, now also replaces the
value of every query parameter and fragment parameter inside a URL (text after `scheme://`, up to
whitespace, a quote or `<`/`>`): `https://h/m.m3u8?sec=abc&t=1` → `https://h/m.m3u8?sec=<redacted>&t=<redacted>`.
Parameter names, paths and hosts stay readable. The existing credential-key masking is unchanged.

## Why
The kit logs media and network URLs at Info/Warn (`controller.cpp` "opened %s", the adaptive source's
error text "HTTP 403 for <url>", `http_stream.cpp`, `segment_reader.cpp`, the image service). Plugin
links are signed URLs whose signature parameter names vary by site (`sec=` on Dailymotion's CDN in the
host run of 2026-10-19), so the key list could not catch them and full tokens reached the log. The
project rule is that secrets and tokens are never logged; doing it once in `redact()` covers every
call site, including future ones.

## Tests
`tests/unit/test_log.cpp` "redact masks every query value of a URL"; the whole host suite
(`ctest --preset host-debug`, 103 tests) passes.
