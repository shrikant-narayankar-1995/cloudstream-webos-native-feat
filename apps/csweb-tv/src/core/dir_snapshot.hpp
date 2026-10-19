// SPDX-License-Identifier: GPL-3.0-or-later
//
// A directory's small files as one JSON object, and back: how plugin data (SharedPreferences, files)
// outlives tmpfs. Only regular files under the named subdirectories are taken; symlinks, special
// files and anything over the limits are skipped. Restoring accepts only plain relative paths
// (no "..", no absolute paths, no empty or "." parts) under those subdirectories.
#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace csweb::core {

struct SnapshotLimits {
    size_t max_files = 500;
    size_t max_file_bytes = 512u << 10;
    size_t max_total_bytes = 2u << 20;
};

// {"version":1,"files":{"shared_prefs/a.json":"<base64>",...}}; `skipped` counts what was left out.
nlohmann::json snapshot_dir(const std::string &root, const std::vector<std::string> &subdirs,
                            const SnapshotLimits &limits = {}, size_t *skipped = nullptr);
// Writes the files of `snap` under root (files there that the snapshot lacks are left alone).
// Returns how many were written; `rejected` counts unsafe or malformed entries.
size_t restore_dir(const std::string &root, const nlohmann::json &snap, const std::vector<std::string> &subdirs,
                   size_t *rejected = nullptr);

bool safe_relative_path(const std::string &path, const std::vector<std::string> &subdirs);

std::string base64_encode(const std::string &data);
bool base64_decode(const std::string &text, std::string *out);

}  // namespace csweb::core
