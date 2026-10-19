// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream's repository model, parsed exactly as the Android app reads it
// (app/.../plugins/RepositoryManager.kt @ eb962b4):
//   repo.json      Repository { name, description?, iconUrl?, manifestVersion, pluginLists[] }
//   plugins.json   [SitePlugin { url, status, version, apiVersion, name, internalName, authors[],
//                   description?, repositoryUrl?, tvTypes[]?, language?, iconUrl?, fileSize?, fileHash? }]
// and the rules around them: what the user may type as a repository (parseRepoUrl), when an
// installed plugin is outdated, and when a plugin is disabled (status DOWN).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace csweb::plugins {

// PROVIDER_STATUS_* (MainAPI.kt)
constexpr int kStatusDown = 0, kStatusOk = 1, kStatusSlow = 2, kStatusBetaOnly = 3;
// PluginManager.kt
constexpr int kVersionAlwaysUpdate = -1;

// A repository the user added (CloudStream RepositoryData).
struct RepositoryData {
    std::string name, url, icon_url;
};

struct Repository {
    std::string name, description, icon_url;
    int manifest_version = 1;
    std::vector<std::string> plugin_lists;
};

struct SitePlugin {
    std::string url;
    int status = kStatusOk;
    int version = 0;
    int api_version = 1;
    std::string name, internal_name, description, repository_url, language, icon_url;
    std::vector<std::string> authors, tv_types;
    long long file_size = -1;
    std::string file_hash;  // "sha256-<hex>" when the list provides it
    bool disabled() const { return status == kStatusDown; }
};

// RepositoryManager.parseRepoUrl: what a typed repository string means.
struct RepoInput {
    enum class Kind { Url, Redirect, Invalid } kind = Kind::Invalid;
    std::string url;  // Url: the repository URL; Redirect: the URL whose Location header is the repository
};
RepoInput parse_repo_input(const std::string &typed);
// The Location of a shortcode redirect, or empty when it points at the service's 404 / home page.
std::string accept_shortcode_redirect(const std::string &redirect_url, const std::string &location);

std::optional<Repository> parse_repository(const std::string &body, std::string *error);
std::vector<SitePlugin> parse_plugin_list(const std::string &body, std::string *error);

// OnlinePluginData.isOutdated
bool is_outdated(int online_version, int installed_version);

// The expected SHA-256 hex from a SitePlugin.fileHash ("sha256-<hex>"), or empty.
std::string expected_sha256(const SitePlugin &p);

// Stable id of an installed plugin: CloudStream keys plugins by (repository URL, internal name).
std::string plugin_id(const std::string &repository_url, const std::string &internal_name);

}  // namespace csweb::plugins
