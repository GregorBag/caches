#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

import abstract_cache;
import arc_cache;
import lfu_cache;
import lirs_cache;
import lru_cache;
import multi_level_cache;
import two_queue_cache;

namespace {

using Level = caches::abstract_cache<int>;
using Levels = std::vector<std::unique_ptr<Level>>;

Levels lru_levels(std::initializer_list<std::size_t> capacities)
{
    Levels levels;
    for (auto capacity : capacities)
        levels.push_back(std::make_unique<caches::lru_cache<int>>(capacity));
    return levels;
}

TEST(MultiLevelCacheTest, EmptyHierarchyDoesNotLoad)
{
    caches::multi_level_cache<int> cache(Levels{});
    Level& base = cache;
    int loads = 0;
    EXPECT_EQ(base.capacity(), 0u);
    EXPECT_TRUE(base.full());
    EXPECT_EQ(base.peek(1), nullptr);
    base.touch(1);
    base.invalidate(1);
    EXPECT_FALSE(base.lookup_update(1, [&](int key) { ++loads; return key; }));
    EXPECT_EQ(loads, 0);
}

TEST(MultiLevelCacheTest, BaseInterfacePreservesPeekTouchAndInvalidationSemantics)
{
    auto levels = lru_levels({1, 3});
    const Level* l1 = levels[0].get();
    const Level* l2 = levels[1].get();
    caches::multi_level_cache<int> cache(std::move(levels));
    Level& base = cache;
    auto load = [](int key) { return key * 10; };

    EXPECT_EQ(base.capacity(), 3u);
    EXPECT_FALSE(base.full());
    for (int key : {1, 2, 3}) EXPECT_FALSE(base.lookup_update(key, load));
    EXPECT_TRUE(base.full());

    ASSERT_NE(base.peek(1), nullptr);
    EXPECT_EQ(*base.peek(1), 10);
    EXPECT_EQ(l1->peek(1), nullptr);
    EXPECT_NE(l1->peek(3), nullptr);

    base.touch(99);
    EXPECT_EQ(base.peek(99), nullptr);
    base.touch(1);
    ASSERT_NE(l1->peek(1), nullptr);
    EXPECT_EQ(*l1->peek(1), 10);
    EXPECT_NE(l2->peek(1), nullptr);

    base.invalidate(1);
    EXPECT_EQ(base.peek(1), nullptr);
    EXPECT_EQ(l1->peek(1), nullptr);
    EXPECT_EQ(l2->peek(1), nullptr);
    EXPECT_FALSE(base.full());
    base.invalidate(1);
    EXPECT_FALSE(base.lookup_update(1, load));
    EXPECT_TRUE(base.full());
}

TEST(MultiLevelCacheTest, NestedHierarchyReportsOnlyDeepestEvictions)
{
    Levels inner_levels;
    inner_levels.push_back(std::make_unique<caches::two_queue_cache<int>>(2));
    inner_levels.push_back(std::make_unique<caches::two_queue_cache<int>>(3));
    const Level* inner_top = inner_levels[0].get();
    const Level* inner_bottom = inner_levels[1].get();

    auto levels = lru_levels({2});
    const Level* outer_top = levels[0].get();
    levels.push_back(std::make_unique<caches::multi_level_cache<int>>(std::move(inner_levels)));
    caches::multi_level_cache<int> cache(std::move(levels));
    auto load = [](int key) { return key * 10; };

    for (int key : {1, 2, 1, 3}) cache.lookup_update(key, load);
    // Eviction from the inner upper level must not invalidate the outer copy.
    EXPECT_EQ(inner_top->peek(1), nullptr);
    EXPECT_NE(inner_bottom->peek(1), nullptr);
    EXPECT_NE(outer_top->peek(1), nullptr);

    EXPECT_FALSE(cache.lookup_update(4, load));
    // Eviction from the inner deepest level must invalidate every upper copy.
    EXPECT_EQ(inner_bottom->peek(1), nullptr);
    EXPECT_EQ(outer_top->peek(1), nullptr);
    EXPECT_EQ(cache.peek(1), nullptr);
    for (const Level* level : {outer_top, inner_top, inner_bottom})
    {
        ASSERT_NE(level->peek(4), nullptr);
        EXPECT_EQ(*level->peek(4), 40);
    }
}

TEST(MultiLevelCacheTest, RejectsNullAndZeroCapacityLevels)
{
    Levels null_levels;
    null_levels.push_back(nullptr);
    EXPECT_THROW(caches::multi_level_cache<int>{std::move(null_levels)}, std::invalid_argument);
    EXPECT_THROW(caches::multi_level_cache<int>{lru_levels({2, 0, 4})}, std::invalid_argument);
}

TEST(MultiLevelCacheTest, RejectsDecreasingCapacities)
{
    EXPECT_THROW(caches::multi_level_cache<int>{lru_levels({3, 2})}, std::invalid_argument);
    EXPECT_THROW(caches::multi_level_cache<int>{lru_levels({2, 4, 3})}, std::invalid_argument);
    EXPECT_THROW(caches::multi_level_cache<int>{lru_levels({4, 2, 8})}, std::invalid_argument);
}

TEST(MultiLevelCacheTest, AcceptsNondecreasingCapacities)
{
    EXPECT_NO_THROW(caches::multi_level_cache<int>{lru_levels({2, 3, 4})});
    EXPECT_NO_THROW(caches::multi_level_cache<int>{lru_levels({2, 2, 2})});
    EXPECT_NO_THROW(caches::multi_level_cache<int>{lru_levels({2, 2, 4})});
}

TEST(MultiLevelCacheTest, SingleLevelMatchesStandaloneCache)
{
    caches::multi_level_cache<int> cache(lru_levels({2}));
    caches::lru_cache<int> standalone(2);
    auto load = [](int key) { return key * 10; };
    for (int key : {1, 2, 1, 3, 2, 3, 4, 4, 1})
        EXPECT_EQ(cache.lookup_update(key, load), standalone.lookup_update(key, load));
}

TEST(MultiLevelCacheTest, LoadsOnceAndPromotesFromMiddleAndDeepestLevels)
{
    auto levels = lru_levels({1, 2, 3});
    const Level* l1 = levels[0].get();
    const Level* l2 = levels[1].get();
    const Level* l3 = levels[2].get();
    caches::multi_level_cache<int> cache(std::move(levels));
    int loads = 0;
    auto load = [&](int key) { ++loads; return key * 10; };

    for (int key : {1, 2, 3})
    {
        EXPECT_FALSE(cache.lookup_update(key, load));
        for (const Level* level : {l1, l2, l3})
        {
            ASSERT_NE(level->peek(key), nullptr);
            EXPECT_EQ(*level->peek(key), key * 10);
        }
    }

    EXPECT_EQ(l1->peek(2), nullptr);
    ASSERT_NE(l2->peek(2), nullptr);
    EXPECT_TRUE(cache.lookup_update(2, load));
    ASSERT_NE(l1->peek(2), nullptr);
    EXPECT_EQ(*l1->peek(2), 20);

    EXPECT_EQ(l1->peek(1), nullptr);
    EXPECT_EQ(l2->peek(1), nullptr);
    ASSERT_NE(l3->peek(1), nullptr);
    EXPECT_TRUE(cache.lookup_update(1, load));
    for (const Level* level : {l1, l2, l3})
    {
        ASSERT_NE(level->peek(1), nullptr);
        EXPECT_EQ(*level->peek(1), 10);
    }
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_EQ(loads, 3);
}

TEST(MultiLevelCacheTest, MiddleEvictionInvalidatesAboveButPreservesBelow)
{
    auto levels = lru_levels({2, 3, 4});
    // 2Q leaves hits in A1in in FIFO order, even when L1 keeps the key hot.
    levels[1] = std::make_unique<caches::two_queue_cache<int>>(3);
    const Level* l1 = levels[0].get();
    const Level* l2 = levels[1].get();
    const Level* l3 = levels[2].get();
    caches::multi_level_cache<int> cache(std::move(levels));
    auto load = [](int key) { return key; };

    for (int key : {1, 2, 1, 3, 1}) cache.lookup_update(key, load);
    ASSERT_NE(l1->peek(1), nullptr);
    EXPECT_FALSE(cache.lookup_update(4, load));
    EXPECT_EQ(l1->peek(1), nullptr);
    EXPECT_EQ(l2->peek(1), nullptr);
    EXPECT_NE(l3->peek(1), nullptr);
    EXPECT_NE(l1->peek(3), nullptr);
    EXPECT_NE(l1->peek(4), nullptr);
}

TEST(MultiLevelCacheTest, DeepestEvictionInvalidatesAllUpperCopies)
{
    auto levels = lru_levels({2, 4, 4});
    levels[2] = std::make_unique<caches::two_queue_cache<int>>(4);
    const Level* l1 = levels[0].get();
    const Level* l2 = levels[1].get();
    const Level* l3 = levels[2].get();
    caches::multi_level_cache<int> cache(std::move(levels));
    auto load = [](int key) { return key; };

    for (int key : {1, 2, 1, 3, 1, 4, 1}) cache.lookup_update(key, load);
    ASSERT_NE(l1->peek(1), nullptr);
    ASSERT_NE(l2->peek(1), nullptr);
    EXPECT_FALSE(cache.lookup_update(5, load));
    for (const Level* level : {l1, l2, l3})
    {
        EXPECT_EQ(level->peek(1), nullptr);
        ASSERT_NE(level->peek(5), nullptr);
        EXPECT_EQ(*level->peek(5), 5);
    }
}

TEST(MultiLevelCacheTest, L1HitRefreshesEveryLevel)
{
    auto levels = lru_levels({2, 2, 2});
    std::vector<const Level*> observed;
    for (const auto& level : levels) observed.push_back(level.get());
    caches::multi_level_cache<int> cache(std::move(levels));
    auto load = [](int key) { return key; };

    EXPECT_FALSE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(2, load));
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(3, load));

    for (const Level* level : observed)
    {
        EXPECT_NE(level->peek(1), nullptr);
        EXPECT_EQ(level->peek(2), nullptr);
        EXPECT_NE(level->peek(3), nullptr);
    }
}

TEST(MultiLevelCacheTest, MiddleHitRefreshesLowerLevels)
{
    auto levels = lru_levels({1, 2, 2});
    const Level* l1 = levels[0].get();
    const Level* l2 = levels[1].get();
    const Level* l3 = levels[2].get();
    caches::multi_level_cache<int> cache(std::move(levels));
    auto load = [](int key) { return key; };

    EXPECT_FALSE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(2, load));
    ASSERT_EQ(l1->peek(1), nullptr);
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(3, load));

    for (const Level* level : {l2, l3})
    {
        EXPECT_NE(level->peek(1), nullptr);
        EXPECT_EQ(level->peek(2), nullptr);
        EXPECT_NE(level->peek(3), nullptr);
    }
    EXPECT_NE(l1->peek(3), nullptr);
}

TEST(MultiLevelCacheTest, MixedPoliciesPreserveInclusionAndValues)
{
    Levels levels;
    levels.push_back(std::make_unique<caches::lru_cache<int>>(2));
    levels.push_back(std::make_unique<caches::lfu_cache<int>>(3));
    levels.push_back(std::make_unique<caches::two_queue_cache<int>>(4));
    levels.push_back(std::make_unique<caches::arc_cache<int>>(5));
    levels.push_back(std::make_unique<caches::lirs_cache<int>>(6));
    std::vector<const Level*> observed;
    for (const auto& level : levels) observed.push_back(level.get());
    caches::multi_level_cache<int> cache(std::move(levels));

    for (int key : {1, 2, 1, 3, 4, 2, 5, 6, 1, 7, 3, 8, 2, 9, 1, 4, 8, 3, 6, 2})
    {
        SCOPED_TRACE(key);
        bool expected_hit = observed.back()->peek(key) != nullptr;
        int loads = 0;
        EXPECT_EQ(cache.lookup_update(key, [&](int k) { ++loads; return k * 10; }), expected_hit);
        EXPECT_EQ(loads, expected_hit ? 0 : 1);
        for (std::size_t i = 0; i < observed.size(); ++i)
        {
            ASSERT_NE(observed[i]->peek(key), nullptr);
            for (int resident = 1; resident <= 9; ++resident)
            {
                if (const int* value = observed[i]->peek(resident))
                {
                    EXPECT_EQ(*value, resident * 10);
                    if (i + 1 < observed.size())
                        EXPECT_NE(observed[i + 1]->peek(resident), nullptr);
                }
            }
        }
    }
}

} // namespace
