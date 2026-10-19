// SPDX-License-Identifier: GPL-3.0-or-later
//
// Media and plugin errors quote the URL they failed on, and stream URLs carry signed tokens in their
// query. Anything such a message is logged or shown with goes through this first.
#pragma once

#include <string>

namespace csweb::core {

// Every "?query" (and "#fragment") of a URL inside `text` becomes "?…".
inline std::string redact_queries(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    bool in_url = false, skipping = false;
    for (size_t i = 0; i < text.size(); i++) {
        const char c = text[i];
        if (c == ' ' || c == '\n' || c == '\t' || c == '"' || c == '\'') {
            in_url = skipping = false;
        } else if (!in_url && text.compare(i, 3, "://") == 0) {
            in_url = true;
        } else if (in_url && !skipping && (c == '?' || c == '#')) {
            skipping = true;
            out += "?\xE2\x80\xA6";
            continue;
        }
        if (!skipping) out += c;
    }
    return out;
}

}  // namespace csweb::core
