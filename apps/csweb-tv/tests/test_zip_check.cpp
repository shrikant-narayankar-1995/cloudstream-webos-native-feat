// SPDX-License-Identifier: GPL-3.0-or-later
// core::check_zip / check_plugin_archive on archives built here byte by byte (stored entries).
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/zip_check.hpp"

#include <string>
#include <vector>

using namespace csweb::core;

namespace {

void le16(std::string &s, unsigned v) { s.push_back(char(v & 0xff)); s.push_back(char(v >> 8 & 0xff)); }
void le32(std::string &s, uint32_t v) { for (int i = 0; i < 4; i++) s.push_back(char(v >> (8 * i) & 0xff)); }

struct Entry {
    std::string name, data;
    uint32_t declared_size = 0xFFFFFFFEu;  // default: data.size()
    uint16_t made_by = 20;
    uint32_t ext_attr = 0;
    uint16_t flags = 0;
};

std::string zip(const std::vector<Entry> &entries) {
    std::string out, central;
    for (const auto &e : entries) {
        const uint32_t offset = uint32_t(out.size());
        const uint32_t usize = e.declared_size == 0xFFFFFFFEu ? uint32_t(e.data.size()) : e.declared_size;
        le32(out, 0x04034b50); le16(out, 20); le16(out, e.flags); le16(out, 0); le16(out, 0); le16(out, 0);
        le32(out, 0); le32(out, uint32_t(e.data.size())); le32(out, usize);
        le16(out, unsigned(e.name.size())); le16(out, 0);
        out += e.name;
        out += e.data;
        le32(central, 0x02014b50); le16(central, e.made_by); le16(central, 20); le16(central, e.flags); le16(central, 0);
        le16(central, 0); le16(central, 0); le32(central, 0); le32(central, uint32_t(e.data.size())); le32(central, usize);
        le16(central, unsigned(e.name.size())); le16(central, 0); le16(central, 0); le16(central, 0); le16(central, 0);
        le32(central, e.ext_attr); le32(central, offset);
        central += e.name;
    }
    const uint32_t cd_offset = uint32_t(out.size());
    out += central;
    le32(out, 0x06054b50); le16(out, 0); le16(out, 0); le16(out, unsigned(entries.size())); le16(out, unsigned(entries.size()));
    le32(out, uint32_t(central.size())); le32(out, cd_offset); le16(out, 0);
    return out;
}

}  // namespace

TEST("plugin_ok") {
    auto r = check_plugin_archive(zip({{"manifest.json", "{}"}, {"classes.dex", "dex\n035"}, {"res/x.xml", "a"}}));
    CHECK(r.ok);
    CHECK_EQ(r.entries, size_t(3));
    CHECK(r.has_manifest && r.has_dex);
}

TEST("multidex_ok_and_missing_parts") {
    CHECK(check_plugin_archive(zip({{"manifest.json", "{}"}, {"classes2.dex", "d"}})).ok);
    CHECK(!check_plugin_archive(zip({{"classes.dex", "d"}})).ok);
    CHECK(!check_plugin_archive(zip({{"manifest.json", "{}"}})).ok);
    CHECK(!check_plugin_archive(zip({{"manifest.json", "{}"}, {"sub/classes.dex", "d"}})).ok);
}

TEST("traversal_and_absolute_rejected") {
    for (const char *bad : {"../evil", "a/../../evil", "/etc/passwd", "a\\b", "C:x"}) {
        auto r = check_zip(zip({{bad, "x"}}));
        CHECK(!r.ok);
        CHECK_EQ(r.error, std::string("unsafe entry name"));
    }
    CHECK(check_zip(zip({{"a..b/c", "x"}})).ok);  // dots inside a name are fine
}

TEST("symlink_encrypted_bomb_rejected") {
    Entry link{"link", "target"};
    link.made_by = 3 << 8;
    link.ext_attr = 0120777u << 16;
    CHECK_EQ(check_zip(zip({link})).error, std::string("symbolic links are not accepted"));
    Entry enc{"x", "y"};
    enc.flags = 1;
    CHECK_EQ(check_zip(zip({enc})).error, std::string("encrypted entries are not accepted"));
    Entry big{"x", "y"};
    big.declared_size = 40u << 20;
    CHECK_EQ(check_zip(zip({big})).error, std::string("an entry is larger than allowed"));
    ZipLimits small;
    small.max_total_uncompressed = 5;
    CHECK_EQ(check_zip(zip({{"a", "123"}, {"b", "456"}}), small).error, std::string("archive expands past the size limit"));
}

TEST("garbage_and_truncation_rejected") {
    CHECK(!check_zip("").ok);
    CHECK(!check_zip("hello world, this is not a zip archive").ok);
    std::string z = zip({{"manifest.json", "{}"}, {"classes.dex", "d"}});
    CHECK(!check_zip(z.substr(0, z.size() - 30)).ok);
}
