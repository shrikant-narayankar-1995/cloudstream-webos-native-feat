// SPDX-License-Identifier: GPL-3.0-or-later
// core/http_url: repository and plugin URLs sent as OkHttp's HttpUrl sends them.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/http_url.hpp"

using csweb::core::canonical_http_url;

TEST("space_in_a_plugin_file_name") {
    CHECK_EQ(canonical_http_url("https://raw.githubusercontent.com/o/r/builds/My Plugin.cs3"),
             std::string("https://raw.githubusercontent.com/o/r/builds/My%20Plugin.cs3"));
}

TEST("valid_urls_are_unchanged") {
    for (const char *u : {"https://h/a/b.json", "https://h:8443/a%20b?x=1&y=%2F#frag", "http://h", "https://h?q=a+b",
                          "https://u:p@h/p/~a-b_c.d!$&'()*+,;=:@"}) {
        CHECK_EQ(canonical_http_url(u), std::string(u));
    }
}

TEST("path_query_fragment_sets") {
    CHECK_EQ(canonical_http_url("https://h/a|b{c}^`\"<>"), std::string("https://h/a%7Cb%7Bc%7D%5E%60%22%3C%3E"));
    CHECK_EQ(canonical_http_url("https://h/it's?q=it's a"), std::string("https://h/it's?q=it%27s%20a"));
    CHECK_EQ(canonical_http_url("https://h?x y"), std::string("https://h?x%20y"));
    CHECK_EQ(canonical_http_url("https://h/p#a b"), std::string("https://h/p#a%20b"));
}

TEST("whitespace_and_bytes") {
    CHECK_EQ(canonical_http_url("  https://h/x.cs3 \n"), std::string("https://h/x.cs3"));
    CHECK_EQ(canonical_http_url("https://h/a\tb\r\nc"), std::string("https://h/abc"));
    CHECK_EQ(canonical_http_url("https://h/caf\xC3\xA9"), std::string("https://h/caf%C3%A9"));
    CHECK_EQ(canonical_http_url("https://h/100%"), std::string("https://h/100%25"));
    CHECK_EQ(canonical_http_url("https://h/%zz%4"), std::string("https://h/%25zz%254"));
}
