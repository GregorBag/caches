#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

import abstract_cache;
import arc_cache;
import lfu_cache;
import lirs_cache;
import lru_cache;
import two_queue_cache;
import multi_level_exclusive_cache;
import multi_level_inclusive_cache;

namespace {

using Level = caches::abstract_cache<int>;
using caches::CacheTypes;

constexpr CacheTypes policies[]{
    CacheTypes::ARC, CacheTypes::LFU, CacheTypes::LRU,
    CacheTypes::LIRS, CacheTypes::TWO_QUEUE};

TEST(MultiLevelExclusiveCacheTest, EmptyHierarchyDoesNotLoad)
{
    caches::multi_level_exclusive_cache<int> cache({});
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

TEST(MultiLevelExclusiveCacheTest, RejectsInvalidLevels)
{
    EXPECT_THROW((caches::multi_level_exclusive_cache<int>{
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, 0}}), std::invalid_argument);
    EXPECT_THROW((caches::multi_level_exclusive_cache<int>{
        {CacheTypes::LRU, 3}, {CacheTypes::LRU, 2}}), std::invalid_argument);
    EXPECT_THROW((caches::multi_level_exclusive_cache<int>{
        {static_cast<CacheTypes>(99), 1}}), std::invalid_argument);
    EXPECT_THROW((caches::multi_level_exclusive_cache<int>{
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, std::numeric_limits<std::size_t>::max()}}),
        std::invalid_argument);
}

TEST(MultiLevelExclusiveCacheTest, CapacityIsSumOfLevels)
{
    caches::multi_level_exclusive_cache<int> cache({
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}, {CacheTypes::LRU, 3}});
    Level& base = cache;
    auto load = [](int key) { return key * 10; };

    EXPECT_EQ(base.capacity(), 6u);
    for (int key = 1; key <= 6; ++key)
    {
        EXPECT_FALSE(base.full());
        EXPECT_FALSE(base.lookup_update(key, load));
    }
    EXPECT_TRUE(base.full());
    for (int key = 1; key <= 6; ++key)
    {
        ASSERT_NE(base.peek(key), nullptr);
        EXPECT_EQ(*base.peek(key), key * 10);
    }

    EXPECT_FALSE(base.lookup_update(7, load));
    EXPECT_EQ(base.peek(1), nullptr);
    for (int key = 2; key <= 7; ++key) EXPECT_NE(base.peek(key), nullptr);
    EXPECT_TRUE(base.full());
}

TEST(MultiLevelExclusiveCacheTest, PromotionsFromMiddleAndDeepestDoNotLoadOrEvict)
{
    caches::multi_level_exclusive_cache<int> cache({
        {CacheTypes::LRU, 1}, {CacheTypes::LRU, 1}, {CacheTypes::LRU, 1}});
    int loads = 0;
    auto load = [&](int key) { ++loads; return key * 10; };
    for (int key : {1, 2, 3}) EXPECT_FALSE(cache.lookup_update(key, load));

    EXPECT_TRUE(cache.lookup_update(2, load));
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_TRUE(cache.lookup_update(1, load));
    EXPECT_EQ(loads, 3);
    EXPECT_TRUE(cache.full());
    for (int key : {1, 2, 3})
    {
        ASSERT_NE(cache.peek(key), nullptr);
        EXPECT_EQ(*cache.peek(key), key * 10);
    }

    EXPECT_FALSE(cache.lookup_update(4, load));
    EXPECT_EQ(cache.peek(3), nullptr);
    EXPECT_NE(cache.peek(1), nullptr);
    EXPECT_NE(cache.peek(2), nullptr);
    EXPECT_EQ(loads, 4);
}

TEST(MultiLevelExclusiveCacheTest, PeekDoesNotPromoteAndTouchPromotes)
{
    caches::multi_level_exclusive_cache<int> peek_cache({{CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}});
    caches::multi_level_exclusive_cache<int> touch_cache({{CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}});
    auto load = [](int key) { return key * 10; };
    for (int key : {1, 2, 3})
    {
        peek_cache.lookup_update(key, load);
        touch_cache.lookup_update(key, load);
    }

    EXPECT_NE(peek_cache.peek(1), nullptr);
    touch_cache.touch(1);
    touch_cache.touch(99);
    EXPECT_EQ(touch_cache.peek(99), nullptr);
    EXPECT_FALSE(peek_cache.lookup_update(4, load));
    EXPECT_FALSE(touch_cache.lookup_update(4, load));
    EXPECT_EQ(peek_cache.peek(1), nullptr);
    EXPECT_NE(touch_cache.peek(1), nullptr);
    EXPECT_EQ(touch_cache.peek(2), nullptr);
}

TEST(MultiLevelExclusiveCacheTest, L1HitRefreshesItsPolicy)
{
    caches::multi_level_exclusive_cache<int> cache({{CacheTypes::LRU, 2}, {CacheTypes::LRU, 2}});
    auto load = [](int key) { return key; };
    for (int key : {1, 2, 3, 4}) cache.lookup_update(key, load);
    EXPECT_TRUE(cache.lookup_update(3, load));
    for (int key : {5, 6, 7}) EXPECT_FALSE(cache.lookup_update(key, load));
    EXPECT_EQ(cache.peek(4), nullptr);
    EXPECT_NE(cache.peek(3), nullptr);
}

TEST(MultiLevelExclusiveCacheTest, InvalidationFreesSpaceAtEveryLevel)
{
    for (int removed : {1, 2, 3})
    {
        SCOPED_TRACE(removed);
        caches::multi_level_exclusive_cache<int> cache({
            {CacheTypes::LRU, 1}, {CacheTypes::LRU, 1}, {CacheTypes::LRU, 1}});
        Level& base = cache;
        auto load = [](int key) { return key * 10; };
        for (int key : {1, 2, 3}) base.lookup_update(key, load);

        base.invalidate(removed);
        base.invalidate(removed);
        EXPECT_EQ(base.peek(removed), nullptr);
        EXPECT_FALSE(base.full());
        EXPECT_EQ(base.capacity(), 3u);
        EXPECT_FALSE(base.lookup_update(4, load));
        EXPECT_TRUE(base.full());
        for (int key : {1, 2, 3, 4})
            EXPECT_EQ(base.peek(key) != nullptr, key != removed);
    }
}

template <typename Cache>
void check_single_level(CacheTypes type)
{
    caches::multi_level_exclusive_cache<int> cache({{type, 3}});
    Cache standalone(3);
    auto load = [](int key) { return key * 10; };
    std::mt19937 random(42);
    for (int request = 0; request < 1000; ++request)
    {
        int key = random() % 9;
        EXPECT_EQ(cache.lookup_update(key, load), standalone.lookup_update(key, load));
        for (int resident = 0; resident < 9; ++resident)
        {
            ASSERT_EQ(cache.peek(resident) != nullptr, standalone.peek(resident) != nullptr);
            if (cache.peek(resident)) EXPECT_EQ(*cache.peek(resident), resident * 10);
        }
    }
}

TEST(MultiLevelExclusiveCacheTest, SingleLevelMatchesEveryStandalonePolicy)
{
    check_single_level<caches::arc_cache<int>>(CacheTypes::ARC);
    check_single_level<caches::lfu_cache<int>>(CacheTypes::LFU);
    check_single_level<caches::lru_cache<int>>(CacheTypes::LRU);
    check_single_level<caches::lirs_cache<int>>(CacheTypes::LIRS);
    check_single_level<caches::two_queue_cache<int>>(CacheTypes::TWO_QUEUE);
}

TEST(MultiLevelExclusiveCacheTest, LruHierarchyMatchesStandaloneWithTotalCapacity)
{
    caches::multi_level_exclusive_cache<int> cache({
        {CacheTypes::LRU, 2}, {CacheTypes::LRU, 3}, {CacheTypes::LRU, 4}});
    caches::lru_cache<int> standalone(9);
    auto load = [](int key) { return key * 10; };
    std::mt19937 random(123);
    for (int request = 0; request < 2000; ++request)
    {
        int key = random() % 15;
        if (request % 7 == 0)
        {
            cache.invalidate(key);
            standalone.invalidate(key);
        }
        else
            EXPECT_EQ(cache.lookup_update(key, load), standalone.lookup_update(key, load));
        EXPECT_EQ(cache.full(), standalone.full());
        for (int resident = 0; resident < 15; ++resident)
            ASSERT_EQ(cache.peek(resident) != nullptr, standalone.peek(resident) != nullptr);
    }
}

struct TrackedValue {
    inline static int alive = 0;
    const std::string payload;

    explicit TrackedValue(std::string value) : payload(std::move(value)) { ++alive; }
    TrackedValue(const TrackedValue& other) : payload(other.payload) { ++alive; }
    ~TrackedValue() { --alive; }
};

TEST(MultiLevelExclusiveCacheTest, MixedPoliciesKeepExactlyOneCopyAndPreserveLoadedValues)
{
    for (auto first : policies)
    for (auto middle : policies)
    for (auto last : policies)
    {
        SCOPED_TRACE(::testing::PrintToString(first) + "/" +
                     ::testing::PrintToString(middle) + "/" + ::testing::PrintToString(last));
        EXPECT_EQ(TrackedValue::alive, 0);
        caches::multi_level_exclusive_cache<TrackedValue, std::string> cache({
            {first, 2}, {middle, 3}, {last, 4}});
        std::unordered_map<std::string, std::string> loaded;
        int loads = 0;
        auto load = [&](const std::string& key) {
            auto value = key + ":" + std::to_string(++loads);
            loaded[key] = value;
            return TrackedValue(value);
        };
        std::mt19937 random(321);
        for (int request = 0; request < 500; ++request)
        {
            SCOPED_TRACE(request);
            auto key = std::to_string(random() % 13);
            const bool present = cache.peek(key) != nullptr;
            const int previous_loads = loads;
            const int previous_residents = TrackedValue::alive;
            if (request % 11 == 0)
            {
                cache.invalidate(key);
                EXPECT_EQ(cache.peek(key), nullptr);
                EXPECT_EQ(loads, previous_loads);
                EXPECT_EQ(TrackedValue::alive, previous_residents - (present ? 1 : 0));
            }
            else if (request % 7 == 0)
            {
                cache.touch(key);
                EXPECT_EQ(cache.peek(key) != nullptr, present);
                EXPECT_EQ(loads, previous_loads);
                EXPECT_EQ(TrackedValue::alive, previous_residents);
            }
            else
            {
                EXPECT_EQ(cache.lookup_update(key, load), present);
                EXPECT_EQ(loads, previous_loads + (present ? 0 : 1));
                ASSERT_NE(cache.peek(key), nullptr);
                EXPECT_EQ(TrackedValue::alive,
                          previous_residents + (present || previous_residents == 9 ? 0 : 1));
            }

            int residents = 0;
            for (int candidate = 0; candidate < 13; ++candidate)
            {
                auto resident = std::to_string(candidate);
                if (const auto* value = cache.peek(resident))
                {
                    ++residents;
                    EXPECT_EQ(value->payload, loaded.at(resident));
                }
            }
            EXPECT_LE(residents, 9);
            EXPECT_EQ(TrackedValue::alive, residents);
            EXPECT_EQ(cache.full(), residents == 9);
        }
    }
    EXPECT_EQ(TrackedValue::alive, 0);
}

TEST(MultiLevelExclusiveCacheTest, BothHierarchiesCanBeImportedAndUsedTogether)
{
    caches::multi_level_exclusive_cache<int> exclusive({{CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}});
    caches::multi_level_inclusive_cache<int> inclusive({{CacheTypes::LRU, 1}, {CacheTypes::LRU, 2}});
    auto load = [](int key) { return key; };
    for (int key : {1, 2, 3})
    {
        exclusive.lookup_update(key, load);
        inclusive.lookup_update(key, load);
    }
    EXPECT_EQ(exclusive.capacity(), 3u);
    EXPECT_EQ(inclusive.capacity(), 2u);
    EXPECT_TRUE(exclusive.lookup_update(1, load));
    EXPECT_FALSE(inclusive.lookup_update(1, load));
}

} // namespace
