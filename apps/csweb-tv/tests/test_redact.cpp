// SPDX-License-Identifier: GPL-3.0-or-later
// core/redact: signed stream URLs in error messages.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/redact.hpp"

using csweb::core::redact_queries;

TEST("redacts_query_and_fragment") {
    CHECK_EQ(redact_queries("open failed: HTTP 403 for https://h/x.m3u8?sec=abc&t=1 (retry)"),
             std::string("open failed: HTTP 403 for https://h/x.m3u8?\xE2\x80\xA6 (retry)"));
    CHECK_EQ(redact_queries("https://h/a#tok"), std::string("https://h/a?\xE2\x80\xA6"));
}

TEST("leaves_other_text") {
    CHECK_EQ(redact_queries("why? no url here"), std::string("why? no url here"));
    CHECK_EQ(redact_queries("a https://h/p b?c"), std::string("a https://h/p b?c"));
}
