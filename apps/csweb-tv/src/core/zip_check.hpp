// SPDX-License-Identifier: GPL-3.0-or-later
//
// Structural check of a downloaded plugin (.cs3 is a ZIP) before it is stored or handed to the
// runtime. Reads only the central directory; nothing is extracted. Rejects what an archive must not
// contain on this device: paths that escape (`..`, absolute, backslashes, NUL), symlinks, encrypted
// entries, ZIP64 and multi-disk archives, too many entries, and declared uncompressed sizes past a
// limit (a decompression bomb is refused before the runtime opens it).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace csweb::core {

struct ZipLimits {
    size_t max_entries = 4096;
    uint64_t max_total_uncompressed = 64ull << 20;  // 64 MiB
    uint64_t max_entry_uncompressed = 32ull << 20;
};

struct ZipReport {
    bool ok = false;
    std::string error;  // why it was refused (empty when ok)
    size_t entries = 0;
    uint64_t total_uncompressed = 0;
    bool has_manifest = false;  // manifest.json at the root (CloudStream's plugin manifest)
    bool has_dex = false;       // classes.dex or classesN.dex at the root
    std::vector<std::string> names;
};

ZipReport check_zip(std::string_view data, const ZipLimits &limits = {});

// A plugin must also carry a manifest and code.
ZipReport check_plugin_archive(std::string_view data, const ZipLimits &limits = {});

}  // namespace csweb::core
