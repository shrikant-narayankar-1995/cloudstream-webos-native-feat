// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/deep_link.hpp"

#include <nlohmann/json.hpp>

#include <cctype>

namespace csweb::app {

namespace {
std::string url_decode(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += s[i] == '+' ? ' ' : s[i];
        }
    }
    return out;
}
}  // namespace

DeepLink parse_deep_link(const std::string &params_json) {
    DeepLink link;
    const auto j = nlohmann::json::parse(params_json.empty() ? "{}" : params_json, nullptr, false);
    if (!j.is_object()) return link;
    std::string str;
    for (const char *key : {"target", "url"}) {
        if (auto it = j.find(key); it != j.end() && it->is_string()) {
            str = it->get<std::string>();
            break;
        }
    }
    if (str.rfind("https://cs.repo", 0) == 0) {
        const size_t q = str.find('?');
        if (q == std::string::npos || q + 1 >= str.size()) return link;
        link = {DeepLink::Kind::Repository, "https://" + str.substr(q + 1)};
    } else if (str.rfind("cloudstreamrepo://", 0) == 0) {
        link = {DeepLink::Kind::Repository, "https" + str.substr(std::string("cloudstreamrepo").size())};
    } else if (str.rfind("cloudstreamsearch://", 0) == 0) {
        const std::string q = url_decode(str.substr(std::string("cloudstreamsearch://").size()));
        if (!q.empty()) link = {DeepLink::Kind::Search, q};
    }
    return link;
}

}  // namespace csweb::app
