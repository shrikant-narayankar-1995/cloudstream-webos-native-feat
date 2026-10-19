// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/http_url.hpp"

#include <cctype>
#include <cstring>

namespace csweb::core {

namespace {

bool is_hex(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

// HttpUrl.canonicalize with alreadyEncoded = true over [from, to) of `in`.
void canonicalize(const std::string &in, size_t from, size_t to, const char *encode_set, std::string &out) {
    static const char kHex[] = "0123456789ABCDEF";
    for (size_t i = from; i < to; i++) {
        const auto c = static_cast<unsigned char>(in[i]);
        if (c == '\t' || c == '\n' || c == '\f' || c == '\r') continue;
        const bool valid_escape = c == '%' && i + 2 < to && is_hex(in[i + 1]) && is_hex(in[i + 2]);
        if (c < 0x20 || c >= 0x7F || std::strchr(encode_set, c) != nullptr || (c == '%' && !valid_escape)) {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        } else {
            out += static_cast<char>(c);
        }
    }
}

}  // namespace

std::string canonical_http_url(const std::string &url) {
    size_t begin = 0, end = url.size();
    while (begin < end && static_cast<unsigned char>(url[begin]) <= 0x20) begin++;
    while (end > begin && static_cast<unsigned char>(url[end - 1]) <= 0x20) end--;
    const std::string s = url.substr(begin, end - begin);

    // Scheme and authority stay as written; the rest is path, then "?query", then "#fragment".
    const size_t scheme = s.find("://");
    size_t pos = scheme == std::string::npos ? 0 : s.find_first_of("/?#", scheme + 3);
    if (pos == std::string::npos) pos = s.size();
    std::string out = s.substr(0, pos);
    const size_t q = s.find_first_of("?#", pos);
    const size_t path_end = q == std::string::npos ? s.size() : q;
    canonicalize(s, pos, path_end, " \"<>^`{}|", out);
    if (path_end == s.size()) return out;
    size_t frag = s.find('#', path_end);
    if (s[path_end] == '?') {
        const size_t query_end = frag == std::string::npos ? s.size() : frag;
        out += '?';
        canonicalize(s, path_end + 1, query_end, " \"'<>", out);
    } else {
        frag = path_end;
    }
    if (frag != std::string::npos) {
        out += '#';
        canonicalize(s, frag + 1, s.size(), " \"<>`", out);
    }
    return out;
}

}  // namespace csweb::core
