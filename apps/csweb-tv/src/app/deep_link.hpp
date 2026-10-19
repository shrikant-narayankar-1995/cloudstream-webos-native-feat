// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream's app links (MainActivity.handleAppIntentUrl), arriving on webOS as the launch
// parameter "target" (or "url"): cloudstreamrepo://… and https://cs.repo?… add a repository,
// cloudstreamsearch://<query> opens Search with that query. Other links are ignored.
#pragma once

#include <string>

namespace csweb::app {

struct DeepLink {
    enum class Kind { None, Repository, Search } kind = Kind::None;
    std::string value;  // Repository: the https URL; Search: the decoded query
};

// `params_json`: the launch parameters object ("{}" or empty when there are none).
DeepLink parse_deep_link(const std::string &params_json);

}  // namespace csweb::app
