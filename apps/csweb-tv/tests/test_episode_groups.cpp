// SPDX-License-Identifier: GPL-3.0-or-later
// runtime/episode_groups against CloudStream's ResultViewModel2 grouping and getRanges.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "runtime/episode_groups.hpp"

using namespace csweb::runtime;

namespace {
Episode ep(int season, int episode, std::string dub = {}) {
    Episode e;
    e.season = season;
    e.episode = episode;
    e.dub = std::move(dub);
    return e;
}
}  // namespace

TEST("groups_by_dub_and_season") {
    std::vector<Episode> eps{ep(1, 1), ep(1, 2), ep(2, 1), ep(1, 1, "Dubbed"), ep(1, 1, "Subbed")};
    auto g = group_episodes(eps);
    CHECK_EQ(g.size(), size_t(4));
    CHECK_EQ(g[(EpisodeGroup{"", 1})].size(), size_t(2));
    CHECK_EQ(g[(EpisodeGroup{"", 2})].size(), size_t(1));
    CHECK_EQ(g[(EpisodeGroup{"Dubbed", 1})].size(), size_t(1));
}

TEST("missing_episode_number_is_position") {
    std::vector<Episode> eps{ep(0, 0), ep(0, 0), ep(0, 7)};
    auto g = group_episodes(eps)[(EpisodeGroup{"", 0})];
    CHECK_EQ(g[0].episode, 1);
    CHECK_EQ(g[1].episode, 2);
    CHECK_EQ(g[2].episode, 7);
}

TEST("small_group_is_one_range") {
    std::vector<Episode> eps;
    for (int i = 1; i <= 60; i++) eps.push_back(ep(1, i));  // size + 10: still one range
    auto r = episode_ranges(eps, 50);
    REQUIRE(r.size() == 1);
    CHECK_EQ(r[0].length, size_t(60));
    CHECK_EQ(r[0].min_episode, 1);
    CHECK_EQ(r[0].max_episode, 60);
}

TEST("large_group_splits_by_episode_number") {
    std::vector<Episode> eps;
    for (int i = 1; i <= 120; i++) eps.push_back(ep(1, i));
    auto r = episode_ranges(eps, 50);
    REQUIRE(r.size() == 3);
    CHECK_EQ(r[0].start, size_t(0));
    CHECK_EQ(r[0].length, size_t(50));
    CHECK_EQ(r[1].min_episode, 51);
    CHECK_EQ(r[1].max_episode, 100);
    CHECK_EQ(r[2].start, size_t(100));
    CHECK_EQ(r[2].length, size_t(20));
}

TEST("gaps_skip_empty_ranges") {
    std::vector<Episode> eps;
    for (int i = 1; i <= 40; i++) eps.push_back(ep(1, i));
    for (int i = 201; i <= 240; i++) eps.push_back(ep(1, i));  // nothing in 41-200
    auto r = episode_ranges(eps, 50);
    REQUIRE(r.size() == 2);
    CHECK_EQ(r[1].min_episode, 201);
    CHECK_EQ(r[1].max_episode, 240);
}
