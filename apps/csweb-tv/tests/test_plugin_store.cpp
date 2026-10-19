// SPDX-License-Identifier: GPL-3.0-or-later
// plugins/PluginStore on the kit's host file store: records, bytes, hash checks on the way out.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/sha256.hpp"
#include "plugins/plugin_store.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <sys/stat.h>
#include <unistd.h>

using namespace csweb::plugins;

namespace {
struct Dir {
    std::string path;
    Dir() {
        char t[] = "/tmp/csweb-store-XXXXXX";
        path = mkdtemp(t);
        mkdir((path + "/kv").c_str(), 0700);  // the host file store needs its directory
    }
    ~Dir() { std::system(("rm -rf '" + path + "'").c_str()); }
};
}  // namespace

TEST("repositories_round_trip") {
    Dir d;
    auto kv = wnk::storage::open_file_store(d.path + "/kv");
    REQUIRE(kv != nullptr);
    PluginStore s(*kv);
    CHECK(s.repositories().empty());
    std::string err;
    CHECK(s.save_repositories({{"Example", "https://e.org/r.json", ""}}, &err));
    const auto repos = s.repositories();
    REQUIRE(repos.size() == 1);
    CHECK_EQ(repos[0].url, std::string("https://e.org/r.json"));
}

TEST("install_materialize_remove") {
    Dir d;
    auto kv = wnk::storage::open_file_store(d.path + "/kv");
    PluginStore s(*kv);
    const std::string bytes(300000, 'p');
    InstalledPlugin p;
    p.id = plugin_id("https://e.org/r.json", "A");
    p.internal_name = "A";
    p.name = "A";
    p.repository_url = "https://e.org/r.json";
    p.version = 3;
    p.sha256 = csweb::core::Sha256::hex(bytes);
    p.size = static_cast<long long>(bytes.size());
    std::string err;
    REQUIRE(s.install(p, bytes, &err));
    auto list = s.installed();
    REQUIRE(list.size() == 1);
    CHECK_EQ(list[0].version, 3);
    const std::string path = s.materialize(list[0], d.path + "/out", &err);
    REQUIRE(!path.empty());
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    CHECK(ss.str() == bytes);

    InstalledPlugin wrong = list[0];
    wrong.sha256 = std::string(64, '0');
    CHECK(s.materialize(wrong, d.path + "/out", &err).empty());
    CHECK(!s.install(wrong, bytes, &err));  // record hash must match the bytes

    CHECK(s.set_enabled(p.id, false, &err));
    CHECK(!s.installed()[0].enabled);
    CHECK(s.remove(p.id, &err));
    CHECK(s.installed().empty());
}
