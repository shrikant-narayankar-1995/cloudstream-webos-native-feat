// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugins/repository.hpp"

#include "core/sha256.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <regex>

namespace csweb::plugins {

namespace {

using nlohmann::json;

std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

std::string opt_str(const json &o, const char *k) {
    auto it = o.find(k);
    return it != o.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::vector<std::string> str_list(const json &o, const char *k) {
    std::vector<std::string> out;
    auto it = o.find(k);
    if (it == o.end() || !it->is_array()) return out;
    for (const auto &v : *it) if (v.is_string()) out.push_back(v.get<std::string>());
    return out;
}

bool is_http_url(const std::string &s) { return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0; }

}  // namespace

RepoInput parse_repo_input(const std::string &typed) {
    const std::string s = trim(typed);
    RepoInput r;
    static const std::regex kHttp("^https?://");
    static const std::regex kScheme(R"(^(cloudstreamrepo://)|(https://cs\.repo/\??))");
    static const std::regex kShort("^[a-zA-Z0-9!_-]+$");
    // Same order as parseRepoUrl: any http(s) URL is taken as it is (so an https://cs.repo/ link is too,
    // exactly as upstream; only cloudstreamrepo:// reaches the rewrite below).
    if (std::regex_search(s, kHttp)) {
        r.kind = RepoInput::Kind::Url;
        r.url = s;
        return r;
    }
    if (std::regex_search(s, kScheme)) {
        std::string rest = std::regex_replace(s, kScheme, "");
        r.kind = RepoInput::Kind::Url;
        r.url = is_http_url(rest) ? rest : "https://" + rest;
        return r;
    }
    if (std::regex_match(s, kShort)) {
        r.kind = RepoInput::Kind::Redirect;
        r.url = s[0] == '!' ? "https://py.md/" + s.substr(1) : "https://cutt.ly/" + s;
        return r;
    }
    return r;
}

std::string accept_shortcode_redirect(const std::string &redirect_url, const std::string &location) {
    if (location.empty()) return {};
    const std::string base = redirect_url.rfind("https://py.md/", 0) == 0 ? "https://py.md" : "https://cutt.ly";
    if (location.rfind(base + "/404", 0) == 0) return {};
    std::string l = location;
    while (!l.empty() && l.back() == '/') l.pop_back();
    if (l == base) return {};
    return location;
}

std::optional<Repository> parse_repository(const std::string &body, std::string *error) {
    const auto j = json::parse(body, nullptr, false);
    if (!j.is_object() || !j.contains("name") || !j["name"].is_string() || !j.contains("pluginLists") ||
        !j["pluginLists"].is_array()) {
        if (error) *error = "not a CloudStream repository (repo.json needs name and pluginLists)";
        return std::nullopt;
    }
    Repository r;
    r.name = j["name"].get<std::string>();
    r.description = opt_str(j, "description");
    r.icon_url = opt_str(j, "iconUrl");
    if (j.contains("manifestVersion") && j["manifestVersion"].is_number_integer()) r.manifest_version = j["manifestVersion"].get<int>();
    for (const auto &u : j["pluginLists"]) {
        if (u.is_string() && is_http_url(u.get<std::string>())) r.plugin_lists.push_back(u.get<std::string>());
    }
    return r;
}

std::vector<SitePlugin> parse_plugin_list(const std::string &body, std::string *error) {
    std::vector<SitePlugin> out;
    const auto j = json::parse(body, nullptr, false);
    if (!j.is_array()) {
        if (error) *error = "plugin list is not a JSON array";
        return out;
    }
    for (const auto &o : j) {
        // Required by SitePlugin's constructor; an entry without them is skipped like a failed parse.
        if (!o.is_object() || !o.contains("url") || !o["url"].is_string() || !o.contains("internalName") ||
            !o["internalName"].is_string() || !o.contains("version") || !o["version"].is_number_integer()) {
            continue;
        }
        SitePlugin p;
        p.url = o["url"].get<std::string>();
        if (!is_http_url(p.url)) continue;
        p.internal_name = o["internalName"].get<std::string>();
        p.version = o["version"].get<int>();
        p.name = opt_str(o, "name");
        if (p.name.empty()) p.name = p.internal_name;
        if (o.contains("status") && o["status"].is_number_integer()) p.status = o["status"].get<int>();
        if (o.contains("apiVersion") && o["apiVersion"].is_number_integer()) p.api_version = o["apiVersion"].get<int>();
        p.authors = str_list(o, "authors");
        p.description = opt_str(o, "description");
        p.repository_url = opt_str(o, "repositoryUrl");
        p.tv_types = str_list(o, "tvTypes");
        p.language = opt_str(o, "language");
        p.icon_url = opt_str(o, "iconUrl");
        if (o.contains("fileSize") && o["fileSize"].is_number_integer()) p.file_size = o["fileSize"].get<long long>();
        p.file_hash = opt_str(o, "fileHash");
        out.push_back(std::move(p));
    }
    return out;
}

bool is_outdated(int online_version, int installed_version) {
    return online_version > installed_version || online_version == kVersionAlwaysUpdate;
}

std::string expected_sha256(const SitePlugin &p) {
    const std::string prefix = "sha256-";
    if (p.file_hash.rfind(prefix, 0) != 0) return {};
    std::string hex = p.file_hash.substr(prefix.size());
    if (!core::is_sha256_hex(hex)) return {};
    for (auto &c : hex) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return hex;
}

std::string plugin_id(const std::string &repository_url, const std::string &internal_name) {
    return core::Sha256::hex(repository_url + "\n" + internal_name).substr(0, 16);
}

}  // namespace csweb::plugins
