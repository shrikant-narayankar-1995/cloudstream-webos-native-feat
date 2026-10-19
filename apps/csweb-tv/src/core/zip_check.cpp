// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/zip_check.hpp"

namespace csweb::core {

namespace {

uint16_t u16(std::string_view d, size_t at) {
    return uint16_t(uint8_t(d[at]) | uint8_t(d[at + 1]) << 8);
}

uint32_t u32(std::string_view d, size_t at) {
    return uint32_t(uint8_t(d[at])) | uint32_t(uint8_t(d[at + 1])) << 8 | uint32_t(uint8_t(d[at + 2])) << 16 |
           uint32_t(uint8_t(d[at + 3])) << 24;
}

ZipReport fail(ZipReport r, std::string why) {
    r.ok = false;
    r.error = std::move(why);
    return r;
}

bool unsafe_name(const std::string &n) {
    if (n.empty() || n[0] == '/' || n.find('\\') != std::string::npos || n.find('\0') != std::string::npos) return true;
    if (n.size() >= 2 && n[1] == ':') return true;  // drive letter
    size_t start = 0;
    while (start <= n.size()) {
        const size_t end = n.find('/', start);
        const std::string part = n.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "..") return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

bool is_root_dex(const std::string &n) {
    if (n.find('/') != std::string::npos || n.size() < 11) return false;
    if (n.compare(0, 7, "classes") != 0 || n.compare(n.size() - 4, 4, ".dex") != 0) return false;
    for (size_t i = 7; i + 4 < n.size(); i++) {
        if (n[i] < '0' || n[i] > '9') return false;
    }
    return true;
}

}  // namespace

ZipReport check_zip(std::string_view d, const ZipLimits &limits) {
    ZipReport r;
    constexpr uint32_t kEocd = 0x06054b50, kCentral = 0x02014b50;
    if (d.size() < 22) return fail(r, "not a ZIP archive (too short)");
    // The end-of-central-directory record is within the last 64 KiB + 22 bytes (comment limit).
    size_t eocd = std::string::npos;
    const size_t lowest = d.size() > 65557 ? d.size() - 65557 : 0;
    for (size_t at = d.size() - 22 + 1; at-- > lowest;) {
        if (u32(d, at) == kEocd) {
            eocd = at;
            break;
        }
    }
    if (eocd == std::string::npos) return fail(r, "not a ZIP archive (no end of central directory)");
    if (u16(d, eocd + 4) != 0 || u16(d, eocd + 6) != 0) return fail(r, "multi-disk archives are not accepted");
    const uint16_t count = u16(d, eocd + 10);
    const uint32_t cd_size = u32(d, eocd + 12);
    const uint32_t cd_offset = u32(d, eocd + 16);
    if (count == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_offset == 0xFFFFFFFFu) return fail(r, "ZIP64 archives are not accepted");
    if (count > limits.max_entries) return fail(r, "too many entries");
    if (uint64_t(cd_offset) + cd_size > eocd) return fail(r, "central directory out of range");

    size_t p = cd_offset;
    for (uint16_t i = 0; i < count; i++) {
        if (p + 46 > eocd || u32(d, p) != kCentral) return fail(r, "corrupt central directory");
        const uint16_t made_by = u16(d, p + 4);
        const uint16_t flags = u16(d, p + 8);
        const uint32_t csize = u32(d, p + 20);
        const uint32_t usize = u32(d, p + 24);
        const uint16_t name_len = u16(d, p + 28), extra_len = u16(d, p + 30), comment_len = u16(d, p + 32);
        const uint32_t ext_attr = u32(d, p + 38);
        const uint32_t local = u32(d, p + 42);
        if (p + 46 + name_len > eocd) return fail(r, "corrupt central directory");
        std::string name(d.substr(p + 46, name_len));
        if (unsafe_name(name)) return fail(r, "unsafe entry name");
        if (flags & 1u) return fail(r, "encrypted entries are not accepted");
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local == 0xFFFFFFFFu) return fail(r, "ZIP64 entries are not accepted");
        if ((made_by >> 8) == 3 && ((ext_attr >> 16) & 0170000) == 0120000) return fail(r, "symbolic links are not accepted");
        if (local >= cd_offset) return fail(r, "entry data out of range");
        if (usize > limits.max_entry_uncompressed) return fail(r, "an entry is larger than allowed");
        r.total_uncompressed += usize;
        if (r.total_uncompressed > limits.max_total_uncompressed) return fail(r, "archive expands past the size limit");
        if (name == "manifest.json") r.has_manifest = true;
        if (is_root_dex(name)) r.has_dex = true;
        r.names.push_back(std::move(name));
        p += 46u + name_len + extra_len + comment_len;
    }
    r.entries = count;
    r.ok = true;
    return r;
}

ZipReport check_plugin_archive(std::string_view data, const ZipLimits &limits) {
    ZipReport r = check_zip(data, limits);
    if (!r.ok) return r;
    if (!r.has_manifest) return fail(r, "no manifest.json: not a CloudStream plugin");
    if (!r.has_dex) return fail(r, "no classes.dex: not a CloudStream plugin");
    return r;
}

}  // namespace csweb::core
