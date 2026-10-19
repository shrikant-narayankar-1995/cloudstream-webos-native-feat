// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream fetches repositories, plugins and subtitle files with OkHttp, which parses every URL
// with HttpUrl and so accepts URLs as repositories write them: "builds/My Plugin.cs3" is requested
// as "builds/My%20Plugin.cs3". The kit's HTTP client refuses a URL with whitespace instead, so URLs
// from repositories and plugins go through canonical_http_url() first.
#pragma once

#include <string>

namespace csweb::core {

// The URL as OkHttp's HttpUrl would send it, for the parts that differ in practice: surrounding
// ASCII whitespace is trimmed; tab, CR, LF and form feed inside are dropped; in the path, query and
// fragment, spaces, control and non-ASCII bytes and the characters HttpUrl escapes there
// (path: " \"<>^`{}|", query: " \"'<>") become %XX; a '%' not followed by two hex digits becomes
// %25. Scheme, user info, host and port are left as they are.
std::string canonical_http_url(const std::string &url);

}  // namespace csweb::core
