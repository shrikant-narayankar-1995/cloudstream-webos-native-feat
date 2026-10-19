# Kit patch 0006: cookies kept across a transport's requests

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc` after patches 0001-0005.
Patch: `0006-http-cookies.patch` (net/include/wnk/net/http.hpp, net/src/curl_api.hpp, curl_api.cpp,
curl_transport.cpp, tests/integration/http_server.py, test_http_curl.cpp, test_adaptive_source.cpp).

## What
`CurlOptions::cookies`: a transport made with it keeps the cookies servers set (Set-Cookie, including
on redirects) in memory and sends them back on its later requests, through libcurl's own cookie engine
shared between its handles (`curl_share`, cookie data, one lock per data kind). Off by default; nothing
is read from or written to disk; a request's own Cookie header is still sent. The share functions are
loaded as optional symbols: without them the option does nothing.

## Why
CloudStream plays links through ExoPlayer with a Cronet source set to handle Set-Cookie
(`CS3IPlayer.createVideoSource`). Every request already carries the link's headers (User-Agent,
Referer, Origin, ...) in the kit: playlists, segments, keys and subtitles. But each request was
independent, so a stream whose server sets a cookie on the playlist and checks it on the segments
failed with HTTP 403.

## Tests
`test_http_curl.cpp` "cookies: kept across requests (also when set on a redirect) only when the
transport asks"; `test_adaptive_source.cpp` "HLS behind Referer, Origin and a cookie the playlist
sets" (the test server's new `/guard/` route: 250 frames with headers and cookies; HTTP 403 without
either). Whole host suite: 107/107.
