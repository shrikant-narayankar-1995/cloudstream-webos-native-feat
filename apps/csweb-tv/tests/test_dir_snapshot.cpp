// SPDX-License-Identifier: GPL-3.0-or-later
// core/dir_snapshot: round trip, limits and hostile snapshots.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/dir_snapshot.hpp"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace csweb::core;

namespace {
std::string tmpdir() {
    char t[] = "/tmp/csweb-snap-XXXXXX";
    return ::mkdtemp(t);
}
void write(const std::string &p, const std::string &s) { std::ofstream(p, std::ios::binary) << s; }
std::string read(const std::string &p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
const std::vector<std::string> kSubs{"shared_prefs", "files"};
}  // namespace

TEST("base64") {
    for (std::string s : {std::string(), std::string("f"), std::string("fo"), std::string("foo"), std::string("foob"),
                          std::string("\0\xff\x10", 3)}) {
        std::string back;
        REQUIRE(base64_decode(base64_encode(s), &back));
        CHECK_EQ(back, s);
    }
    CHECK_EQ(base64_encode("foobar"), std::string("Zm9vYmFy"));
    std::string x;
    CHECK(!base64_decode("Zm9", &x));
    CHECK(!base64_decode("Zm=v", &x));
}

TEST("round_trip") {
    const std::string a = tmpdir(), b = tmpdir();
    ::mkdir((a + "/shared_prefs").c_str(), 0700);
    ::mkdir((a + "/files").c_str(), 0700);
    ::mkdir((a + "/files/sub").c_str(), 0700);
    ::mkdir((a + "/cache").c_str(), 0700);
    write(a + "/shared_prefs/p.json", "{\"k\":1}");
    write(a + "/files/sub/bin", std::string("\0\1\2", 3));
    write(a + "/cache/skip", "not kept");
    ::symlink("/etc/passwd", (a + "/files/link").c_str());
    size_t skipped = 9;
    auto snap = snapshot_dir(a, kSubs, {}, &skipped);
    CHECK_EQ(skipped, size_t(1));  // the symlink
    CHECK_EQ(snap["files"].size(), size_t(2));
    CHECK_EQ(restore_dir(b, snap, kSubs), size_t(2));
    CHECK_EQ(read(b + "/shared_prefs/p.json"), std::string("{\"k\":1}"));
    CHECK_EQ(read(b + "/files/sub/bin"), std::string("\0\1\2", 3));
}

TEST("limits") {
    const std::string a = tmpdir();
    ::mkdir((a + "/files").c_str(), 0700);
    write(a + "/files/big", std::string(2000, 'x'));
    write(a + "/files/small", "y");
    SnapshotLimits lim;
    lim.max_file_bytes = 1000;
    size_t skipped = 0;
    auto snap = snapshot_dir(a, kSubs, lim, &skipped);
    CHECK_EQ(snap["files"].size(), size_t(1));
    CHECK_EQ(skipped, size_t(1));
}

TEST("hostile_snapshot") {
    const std::string b = tmpdir();
    CHECK(!safe_relative_path("../x", kSubs));
    CHECK(!safe_relative_path("/etc/x", kSubs));
    CHECK(!safe_relative_path("files/../../x", kSubs));
    CHECK(!safe_relative_path("cache/x", kSubs));
    CHECK(!safe_relative_path("files", kSubs));
    CHECK(!safe_relative_path("files//x", kSubs));
    CHECK(safe_relative_path("files/a/b", kSubs));
    ::mkdir((b + "/files").c_str(), 0700);
    ::symlink("/tmp", (b + "/files/out").c_str());  // a planted directory symlink
    nlohmann::json snap = {{"files",
                            {{"../escape", base64_encode("x")},
                             {"files/out/escape", base64_encode("x")},
                             {"files/ok", base64_encode("ok")},
                             {"files/notb64", "@@@@"},
                             {"files/num", 5}}}};
    size_t rejected = 0;
    CHECK_EQ(restore_dir(b, snap, kSubs, &rejected), size_t(1));
    CHECK_EQ(rejected, size_t(4));
    CHECK_EQ(read(b + "/files/ok"), std::string("ok"));
}
