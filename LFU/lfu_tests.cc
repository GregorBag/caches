#include <gtest/gtest.h>
#include <vector>

#include "lfu_cache.hpp"

TEST(LFUCacheTest, CacheHitsTest)
{
    struct TestCase 
    {
        int cache_capacity;
        std::vector<int> keys;
        int hits;
    };

    const std::vector<TestCase> test_cases = {
        // Empty requests, zero capacity, and a cache with one slot.
        { 0, {}, 0 },
        { 0, { 1 }, 0 },
        { 0, { 1, 1, 2, 1, 2 }, 0 },
        { 1, {}, 0 },
        { 4, {}, 0 },
        { 1, { 42 }, 0 },
        { 8, { 42 }, 0 },
        { 1, { 7, 7, 7, 7, 7, 7 }, 5 },
        { 4, { 7, 7, 7, 7, 7, 7 }, 5 },
        { 1, { 1, 2, 1, 2, 1 }, 0 },
        { 1, { 1, 1, 2, 2, 1, 1 }, 3 },
        { 1, { 1, 1, 2, 2, 3, 3, 1, 1, 2, 2, 3, 3 }, 6 },

        // Cache filling, repeated scans, and capacity boundaries.
        { 2, { 1, 2, 1, 2, 1, 2 }, 4 },
        { 3, { 1, 2, 3, 1, 2, 3, 1, 2, 3 }, 6 },
        { 8, { 1, 2, 3, 1, 2, 3, 1, 2, 3 }, 6 },
        { 1000000, { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 }, 5 },
        { 3, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, 0 },
        { 2, { 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3 }, 0 },
        { 3, { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 }, 0 },
        { 4, { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 }, 8 },
        { 3, { 1, 2, 3, 4, 3, 2, 1 }, 2 },

        // Zero, negative, and extreme int key values.
        { 0, { 0, -1, 0, -1 }, 0 },
        { 1, { 0, 0, -1, -1, 0, 0 }, 3 },
        { 3, { -1, 0, 1, -1, 0, 1, -1, 0, 1 }, 6 },
        { 2, { -3, -2, -3, -1, -2, -3, -1, -2 }, 2 },
        { 2, { -2147483648, 2147483647, -2147483648, 2147483647, 0, -2147483648, 2147483647 }, 3 },
        { 4, {
            -2147483648, -1, 0, 2147483647, -2147483648, -1, 0, 2147483647, -2147483648, -1, 0,
            2147483647
        }, 8 },

        // Frequency takes precedence over recency during eviction.
        { 2, { 1, 1, 1, 2, 3, 1 }, 3 },
        { 2, { 1, 1, 1, 2, 3, 2 }, 2 },
        { 3, { 1, 1, 1, 2, 2, 3, 4, 1, 2, 3 }, 5 },
        { 3, { 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 1, 2, 3 }, 8 },
        { 4, { 1, 1, 1, 2, 2, 3, 3, 4, 5, 1, 2, 3, 4 }, 7 },

        // Equal frequencies are resolved by the least recent access.
        { 2, { 1, 2, 3, 2 }, 1 },
        { 2, { 1, 2, 3, 1 }, 0 },
        { 2, { 1, 2, 1, 2, 3, 1 }, 2 },
        { 2, { 1, 2, 1, 2, 3, 2 }, 3 },
        { 2, { 1, 2, 2, 1, 3, 1 }, 3 },
        { 2, { 1, 2, 2, 1, 3, 2 }, 2 },
        { 3, { 1, 2, 3, 3, 2, 1, 4, 1, 2, 3 }, 5 },

        // Hits create frequency groups, join existing groups, and empty old groups.
        { 3, { 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 2, 3 }, 8 },
        { 3, { 1, 2, 3, 1, 1, 2, 2, 3, 3, 4, 2, 3 }, 8 },
        { 3, { 1, 2, 3, 1, 1, 1, 2, 2, 3, 4, 1, 2 }, 8 },
        { 2, { 1, 2, 1, 2, 1, 2, 1, 2, 3, 2 }, 7 },

        // The minimum frequency advances on hits and resets on insertion.
        { 1, { 1, 1, 1, 1, 1, 2, 2, 3, 3 }, 6 },
        { 2, { 1, 2, 1, 2, 3, 4, 2, 4 }, 4 },
        { 2, { 1, 2, 1, 2, 1, 2, 3, 3, 4, 4, 2, 3 }, 7 },
        { 3, { 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 1, 2, 3 }, 11 },
        { 3, { 1, 2, 3, 1, 2, 3, 4, 4, 5, 5, 6, 6, 1, 2, 3 }, 6 },

        // Evicted keys start at frequency one when inserted again.
        { 2, { 1, 1, 2, 2, 2, 3, 1, 4, 1 }, 3 },
        { 2, { 1, 1, 2, 2, 2, 3, 1, 4, 2 }, 4 },
        { 3, { 1, 1, 2, 2, 2, 3, 3, 3, 4, 1, 5, 1, 2, 3 }, 7 },
        { 2, { 1, 2, 1, 2, 3, 1, 1, 4, 2, 3, 4 }, 3 },

        // Frequently accessed entries survive scans and changes in the working set.
        { 2, { 1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 1, 1 }, 4 },
        { 3, { 1, 2, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2 }, 6 },
        { 4, { 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3 }, 9 },
        { 3, { 1, 2, 1, 2, 3, 4, 5, 3, 4, 5, 3, 4, 5, 1, 2 }, 4 },
        { 4, { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 1, 2, 3, 4 }, 11 },

        // Repeated hits raise frequencies well beyond the cache capacity.
        { 2, {
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 2, 3, 1, 2, 3, 1
        }, 33 },
        { 3, {
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
            3, 3, 3, 3, 3, 3, 3, 4, 5, 1, 2, 3
        }, 35 },
    };

    auto load = [](int key) { return key; };

    for (auto test : test_cases)
    {
        SCOPED_TRACE("test vector: " + ::testing::PrintToString(test.keys));

        caches::lfu_cache_t<int, int> cache(test.cache_capacity);
        int hits = 0;
        for (auto key : test.keys)
        {
            auto hit = cache.lookup_update(key, load);
            if (hit)
                hits++;
        }

        EXPECT_EQ(hits, test.hits);
    }
}
