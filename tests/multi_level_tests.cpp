#include <gtest/gtest.h>
#include <stdexcept>

import abstract_cache;
import lirs_cache;
import lru_cache;
import multi_level_cache;

namespace {

using Level = caches::abstract_cache<int>;
using caches::CacheTypes;

TEST(MultiLevelCacheTest, EmptyHierarchyDoesNotLoad)
{
    caches::multi_level_cache<int> cache({});
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
    caches::multi_level_cache<int> cache({{CacheTypes::LRU, 1}, {CacheTypes::LRU, 3}});
    Level& base = cache;
    auto load = [](int key) { return key * 10; };

    EXPECT_EQ(base.capacity(), 3u);
    EXPECT_FALSE(base.full());
    for (int key : {1, 2, 3}) EXPECT_FALSE(base.lookup_update(key, load));
    EXPECT_TRUE(base.full());

    ASSERT_NE(base.peek(1), nullptr);
    EXPECT_EQ(*base.peek(1), 10);
    EXPECT_NE(base.peek(3), nullptr);

    base.touch(99);
    EXPECT_EQ(base.peek(99), nullptr);
    base.touch(1);
    ASSERT_NE(base.peek(1), nullptr);
    EXPECT_EQ(*base.peek(1), 10);

    base.invalidate(1);
    EXPECT_EQ(base.peek(1), nullptr);
    EXPECT_FALSE(base.full());
    base.invalidate(1);
    EXPECT_FALSE(base.lookup_update(1, load));
    EXPECT_TRUE(base.full());
}

TEST(MultiLevelCacheTest, RejectsZeroCapacityLevels)
{
    EXPECT_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 0}, {CacheTypes::LRU, 4}}),
        std::invalid_argument);
}

TEST(MultiLevelCacheTest, RejectsDecreasingCapacities)
{
    EXPECT_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 3}, {CacheTypes::LRU, 2}}), std::invalid_argument);
    EXPECT_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 4}, {CacheTypes::LRU, 3}}),
        std::invalid_argument);
    EXPECT_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 4}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 8}}),
        std::invalid_argument);
}

TEST(MultiLevelCacheTest, AcceptsNondecreasingCapacities)
{
    EXPECT_NO_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 3}, {CacheTypes::LRU, 4}}));
    EXPECT_NO_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}}));
    EXPECT_NO_THROW((caches::multi_level_cache<int>{
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 4}}));
}

TEST(MultiLevelCacheTest, SingleLevelMatchesStandaloneCache)
{
    caches::multi_level_cache<int> cache({{CacheTypes::LRU, 2}});
    caches::lru_cache<int> standalone(2);
    auto load = [](int key) { return key * 10; };
    for (int key : {1, 2, 1, 3, 2, 3, 4, 4, 1})
        EXPECT_EQ(cache.lookup_update(key, load), standalone.lookup_update(key, load));
}

TEST(MultiLevelCacheTest, LoadsOnceForHitsFromMiddleAndDeepestLevels)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 3}});
    int loads = 0;
    auto load = [&](int key) { ++loads; return key * 10; };

    for (int key : {1, 2, 3})
    {
        EXPECT_FALSE(cache.lookup_update(key, load));
        ASSERT_NE(cache.peek(key), nullptr);
        EXPECT_EQ(*cache.peek(key), key * 10);
    }

    EXPECT_TRUE(cache.lookup_update(2, load));
    ASSERT_NE(cache.peek(2), nullptr);
    EXPECT_EQ(*cache.peek(2), 20);

    EXPECT_TRUE(cache.lookup_update(1, load));
    ASSERT_NE(cache.peek(1), nullptr);
    EXPECT_EQ(*cache.peek(1), 10);
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_EQ(loads, 3);
}

TEST(MultiLevelCacheTest, MiddleEvictionPreservesBelow)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 2}, {CacheTypes::TWO_QUEUE, 3}, {CacheTypes::LRU, 4}});
    // 2Q leaves hits in A1in in FIFO order, even when L1 keeps the key hot.
    auto load = [](int key) { return key; };

    for (int key : {1, 2, 1, 3, 1}) cache.lookup_update(key, load);
    ASSERT_NE(cache.peek(1), nullptr);
    EXPECT_FALSE(cache.lookup_update(4, load));
    EXPECT_NE(cache.peek(1), nullptr);
    EXPECT_NE(cache.peek(3), nullptr);
    EXPECT_NE(cache.peek(4), nullptr);
}

TEST(MultiLevelCacheTest, DeepestEvictionInvalidatesAllUpperCopies)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 4}, {CacheTypes::TWO_QUEUE, 4}});
    auto load = [](int key) { return key; };

    for (int key : {1, 2, 1, 3, 1, 4, 1}) cache.lookup_update(key, load);
    ASSERT_NE(cache.peek(1), nullptr);
    EXPECT_FALSE(cache.lookup_update(5, load));
    EXPECT_EQ(cache.peek(1), nullptr);
    ASSERT_NE(cache.peek(5), nullptr);
    EXPECT_EQ(*cache.peek(5), 5);
}

TEST(MultiLevelCacheTest, L1HitRefreshesEveryLevel)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}});
    auto load = [](int key) { return key; };

    EXPECT_FALSE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(2, load));
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(3, load));

    EXPECT_NE(cache.peek(1), nullptr);
    EXPECT_EQ(cache.peek(2), nullptr);
    EXPECT_NE(cache.peek(3), nullptr);
}

TEST(MultiLevelCacheTest, MiddleHitRefreshesLowerLevels)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}});
    auto load = [](int key) { return key; };

    EXPECT_FALSE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(2, load));
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_FALSE(cache.lookup_update(3, load));

    EXPECT_NE(cache.peek(1), nullptr);
    EXPECT_EQ(cache.peek(2), nullptr);
    EXPECT_NE(cache.peek(3), nullptr);
}

TEST(MultiLevelCacheTest, MixedPoliciesMatchDeepestCacheAndPreserveValues)
{
    caches::multi_level_cache<int> cache({
        {CacheTypes::LRU, 2}, {CacheTypes::LFU, 3}, {CacheTypes::TWO_QUEUE, 4},
        {CacheTypes::ARC, 5}, {CacheTypes::LIRS, 6}});
    // The constructor owns the levels; compare with a standalone deepest-level cache.
    caches::lirs_cache<int> deepest(6);

    for (int key : {1, 2, 1, 3, 4, 2, 5, 6, 1, 7, 3, 8, 2, 9, 1, 4, 8, 3, 6, 2})
    {
        SCOPED_TRACE(key);
        bool expected_hit = deepest.lookup_update(key, [](int k) { return k * 10; });
        int loads = 0;
        EXPECT_EQ(cache.lookup_update(key, [&](int k) { ++loads; return k * 10; }), expected_hit);
        EXPECT_EQ(loads, expected_hit ? 0 : 1);
        ASSERT_NE(cache.peek(key), nullptr);
        for (int resident = 1; resident <= 9; ++resident)
        {
            const int* value = cache.peek(resident);
            const int* expected = deepest.peek(resident);
            ASSERT_EQ(value != nullptr, expected != nullptr);
            if (value) EXPECT_EQ(*value, resident * 10);
        }
    }
}

} // namespace
