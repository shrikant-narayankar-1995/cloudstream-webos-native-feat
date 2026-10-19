// SPDX-License-Identifier: GPL-3.0-or-later
// plugins/repository: CloudStream's repo.json / plugins.json and parseRepoUrl rules.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "plugins/repository.hpp"

using namespace csweb::plugins;

TEST("repo_input_like_parseRepoUrl") {
    auto u = parse_repo_input("  https://example.org/repo.json ");
    CHECK(u.kind == RepoInput::Kind::Url);
    CHECK_EQ(u.url, std::string("https://example.org/repo.json"));
    CHECK_EQ(parse_repo_input("cloudstreamrepo://example.org/r.json").url, std::string("https://example.org/r.json"));
    CHECK_EQ(parse_repo_input("cloudstreamrepo://https://example.org/r.json").url, std::string("https://example.org/r.json"));
    CHECK_EQ(parse_repo_input("https://cs.repo/?https://x.org/r.json").url, std::string("https://cs.repo/?https://x.org/r.json"));
    auto s = parse_repo_input("abc_12-x");
    CHECK(s.kind == RepoInput::Kind::Redirect);
    CHECK_EQ(s.url, std::string("https://cutt.ly/abc_12-x"));
    CHECK_EQ(parse_repo_input("!code").url, std::string("https://py.md/code"));
    CHECK(parse_repo_input("not a url").kind == RepoInput::Kind::Invalid);
    CHECK(parse_repo_input("ftp://x").kind == RepoInput::Kind::Invalid);
}

TEST("shortcode_redirects") {
    CHECK_EQ(accept_shortcode_redirect("https://cutt.ly/x", "https://e.org/r.json"), std::string("https://e.org/r.json"));
    CHECK(accept_shortcode_redirect("https://cutt.ly/x", "https://cutt.ly/404").empty());
    CHECK(accept_shortcode_redirect("https://cutt.ly/x", "https://cutt.ly/").empty());
    CHECK(accept_shortcode_redirect("https://py.md/x", "https://py.md/404?x").empty());
    CHECK(accept_shortcode_redirect("https://cutt.ly/x", "").empty());
}

TEST("repository_json") {
    std::string err;
    auto r = parse_repository(R"J({"name":"Example","description":"d","manifestVersion":1,
        "pluginLists":["https://e.org/plugins.json","javascript:alert(1)",3]})J", &err);
    REQUIRE(r.has_value());
    CHECK_EQ(r->name, std::string("Example"));
    CHECK_EQ(r->plugin_lists.size(), size_t(1));
    CHECK(!parse_repository(R"J({"pluginLists":[]})J", &err).has_value());
    CHECK(!parse_repository("<html>", &err).has_value());
}

TEST("plugin_list_json") {
    std::string err;
    auto list = parse_plugin_list(R"J([
      {"url":"https://e.org/A.cs3","status":1,"version":4,"apiVersion":1,"name":"A","internalName":"AProvider",
       "authors":["x"],"tvTypes":["Movie"],"language":"en","fileSize":1234,
       "fileHash":"sha256-B70462C264CB7F90FC2860A8E58D7544CE747FF347D1D11FA093623901853573"},
      {"url":"https://e.org/B.cs3","status":0,"version":1,"internalName":"B"},
      {"url":"file:///etc/x","version":1,"internalName":"C"},
      {"version":1,"internalName":"NoUrl"}
    ])J", &err);
    REQUIRE(list.size() == 2);
    CHECK_EQ(list[0].name, std::string("A"));
    CHECK_EQ(list[0].file_size, 1234LL);
    CHECK_EQ(expected_sha256(list[0]), std::string("b70462c264cb7f90fc2860a8e58d7544ce747ff347d1d11fa093623901853573"));
    CHECK(list[1].disabled());
    CHECK_EQ(list[1].name, std::string("B"));
    CHECK(expected_sha256(list[1]).empty());
    CHECK(parse_plugin_list("{}", &err).empty());
}

TEST("update_rule_and_ids") {
    CHECK(is_outdated(5, 4));
    CHECK(!is_outdated(4, 4));
    CHECK(is_outdated(-1, 9));
    CHECK_EQ(plugin_id("https://e.org/r.json", "A").size(), size_t(16));
    CHECK(plugin_id("https://e.org/r.json", "A") != plugin_id("https://f.org/r.json", "A"));
}
