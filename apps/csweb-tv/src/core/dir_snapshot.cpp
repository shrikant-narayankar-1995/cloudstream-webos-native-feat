// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/dir_snapshot.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <sstream>

namespace csweb::core {

namespace {
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void walk(const std::string &root, const std::string &rel, nlohmann::json &files, const SnapshotLimits &lim, size_t &total,
          size_t &skipped, int depth) {
    if (depth > 8) return;
    DIR *d = ::opendir((root + "/" + rel).c_str());
    if (!d) return;
    while (dirent *e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        const std::string child = rel + "/" + name;
        struct stat st;
        if (::lstat((root + "/" + child).c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk(root, child, files, lim, total, skipped, depth + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode) || static_cast<size_t>(st.st_size) > lim.max_file_bytes || files.size() >= lim.max_files ||
            total + static_cast<size_t>(st.st_size) > lim.max_total_bytes) {
            ++skipped;
            continue;
        }
        std::ifstream in(root + "/" + child, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string data = ss.str();
        total += data.size();
        files[child] = base64_encode(data);
    }
    ::closedir(d);
}

bool make_parents(const std::string &root, const std::string &rel) {
    size_t pos = 0;
    while ((pos = rel.find('/', pos)) != std::string::npos) {
        const std::string dir = root + "/" + rel.substr(0, pos);
        if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) return false;
        struct stat st;
        if (::lstat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;  // not through a symlink
        ++pos;
    }
    return true;
}
}  // namespace

std::string base64_encode(const std::string &data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const uint32_t v = (uint8_t(data[i]) << 16) | (uint8_t(data[i + 1]) << 8) | uint8_t(data[i + 2]);
        out += kB64[v >> 18];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += kB64[v & 63];
    }
    if (i < data.size()) {
        uint32_t v = uint8_t(data[i]) << 16;
        if (i + 1 < data.size()) v |= uint8_t(data[i + 1]) << 8;
        out += kB64[v >> 18];
        out += kB64[(v >> 12) & 63];
        out += i + 1 < data.size() ? kB64[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

bool base64_decode(const std::string &text, std::string *out) {
    out->clear();
    if (text.size() % 4 != 0) return false;
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    for (size_t i = 0; i < text.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int k = 0; k < 4; k++) {
            const char c = text[i + k];
            if (c == '=' && i + 4 == text.size() && k >= 2) {
                v[k] = 0;
                ++pad;
            } else if (pad > 0 || (v[k] = val(c)) < 0) {
                return false;
            }
        }
        const uint32_t n = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3];
        *out += char(n >> 16);
        if (pad < 2) *out += char((n >> 8) & 255);
        if (pad < 1) *out += char(n & 255);
    }
    return true;
}

bool safe_relative_path(const std::string &path, const std::vector<std::string> &subdirs) {
    if (path.empty() || path[0] == '/' || path.size() > 512) return false;
    std::stringstream ss(path);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == "." || part == ".." || part.find('\0') != std::string::npos) return false;
        parts.push_back(part);
    }
    if (parts.size() < 2 || path.back() == '/') return false;
    for (const auto &s : subdirs) {
        if (parts[0] == s) return true;
    }
    return false;
}

nlohmann::json snapshot_dir(const std::string &root, const std::vector<std::string> &subdirs, const SnapshotLimits &limits,
                            size_t *skipped) {
    nlohmann::json files = nlohmann::json::object();
    size_t total = 0, skip = 0;
    for (const auto &s : subdirs) walk(root, s, files, limits, total, skip, 0);
    if (skipped) *skipped = skip;
    return {{"version", 1}, {"files", files}};
}

size_t restore_dir(const std::string &root, const nlohmann::json &snap, const std::vector<std::string> &subdirs, size_t *rejected) {
    size_t written = 0, bad = 0;
    if (snap.is_object() && snap.contains("files") && snap["files"].is_object()) {
        for (auto it = snap["files"].begin(); it != snap["files"].end(); ++it) {
            std::string data;
            if (!it.value().is_string() || !safe_relative_path(it.key(), subdirs) ||
                !base64_decode(it.value().get<std::string>(), &data) || !make_parents(root, it.key())) {
                ++bad;
                continue;
            }
            // O_NOFOLLOW: never write through a symlink planted at the file's place.
            const int fd = ::open((root + "/" + it.key()).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (fd < 0) {
                ++bad;
                continue;
            }
            const bool ok = ::write(fd, data.data(), data.size()) == static_cast<ssize_t>(data.size());
            ::close(fd);
            ok ? ++written : ++bad;
        }
    }
    if (rejected) *rejected = bad;
    return written;
}

}  // namespace csweb::core
