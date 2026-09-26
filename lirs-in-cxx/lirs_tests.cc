#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "lirs_cache.hpp"

TEST(LIRSCacheTest, CacheHitsTest)
{
    struct TestCase
    {
        int cache_capacity;
        std::vector<int> keys;
        int hits;
    };

    // Prepend a sequential scan of keys [1, last] to the supplied requests.
    // Nested calls express two scans without hundreds of repeated literals.
    const auto after_scan = [](int last, const std::vector<int>& tail) {
        std::vector<int> keys;
        keys.reserve(static_cast<std::size_t>(last) + tail.size());
        for (int key = 1; key <= last; ++key)
            keys.push_back(key);
        keys.insert(keys.end(), tail.begin(), tail.end());
        return keys;
    };

    // Expected hits follow LIRS with HIR capacity max(1, capacity / 100).
    // Metadata memory limits are deliberately outside the scope of these cases.
    const std::vector<TestCase> test_cases = {
        // Empty requests, zero capacity, and single-slot replacement.
        { 0, {}, 0 },
        { 0, { 1 }, 0 },
        { 0, { 1, 1, 2, 1, 2 }, 0 },
        { 1, {}, 0 },
        { 2, {}, 0 },
        { 8, {}, 0 },
        { 1, { 42 }, 0 },
        { 8, { 42 }, 0 },
        { 1, { 7, 7, 7, 7, 7, 7 }, 5 },
        { 4, { 7, 7, 7, 7, 7, 7 }, 5 },
        { 1, { 1, 2 }, 0 },
        { 1, { 1, 2, 1, 2, 1 }, 0 },
        { 1, { 1, 1, 2, 2, 1, 1 }, 3 },
        { 1, { 1, 1, 2, 2, 3, 3, 1, 1, 2, 2, 3, 3 }, 6 },

        // Warm-up, partial occupancy, and working sets that fit in the cache.
        { 2, { 1, 2 }, 0 },
        { 3, { 1, 1, 2, 2, 3, 3 }, 3 },
        { 4, { 1, 2, 2, 1, 3, 3, 4, 4 }, 4 },
        { 2, { 1, 2, 1, 2, 1, 2 }, 4 },
        { 3, { 1, 2, 3, 1, 2, 3, 1, 2, 3 }, 6 },
        { 4, { 1, 2, 3, 4, 4, 3, 2, 1, 1, 2, 3, 4 }, 8 },
        { 8, { 1, 2, 3, 1, 2, 3, 3, 2, 1 }, 6 },
        { 1000000, { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 }, 5 },

        // LIR hits at the top, middle, and bottom change the next demotion.
        { 4, { 1, 2, 3, 4, 3, 3, 4, 5, 3 }, 4 },
        { 4, { 1, 2, 3, 4, 2, 4, 5, 1 }, 2 },
        { 4, { 1, 2, 3, 4, 2, 4, 5, 2 }, 3 },
        { 3, { 1, 2, 3, 1, 3, 4, 1 }, 3 },
        { 3, { 1, 2, 3, 1, 3, 4, 2 }, 2 },
        { 3, { 1, 2, 3, 2, 3, 4, 1 }, 2 },
        { 3, { 1, 2, 3, 2, 3, 4, 2 }, 3 },

        // Resident HIR in S is promoted; the oldest LIR stays resident in Q.
        { 2, { 1, 2, 2 }, 1 },
        { 2, { 1, 2, 2, 1 }, 2 },
        { 2, { 1, 2, 2, 3, 1 }, 1 },
        { 2, { 1, 2, 2, 3, 2 }, 2 },
        { 3, { 1, 2, 3, 3, 1 }, 2 },
        { 3, { 1, 2, 3, 3, 4, 1 }, 1 },
        { 3, { 1, 2, 3, 3, 4, 2 }, 2 },
        { 3, { 1, 2, 3, 3, 3, 3, 4, 3 }, 4 },

        // Nonresident HIR in S is a miss, followed by promotion to LIR.
        { 2, { 1, 2, 3, 2 }, 0 },
        { 2, { 1, 2, 3, 2, 2 }, 1 },
        { 2, { 1, 2, 3, 2, 1 }, 1 },
        { 2, { 1, 2, 3, 2, 3 }, 0 },
        { 2, { 1, 2, 3, 2, 4, 1 }, 0 },
        { 2, { 1, 2, 3, 2, 4, 2 }, 1 },
        { 3, { 1, 2, 3, 4, 3 }, 0 },
        { 3, { 1, 2, 3, 4, 3, 1 }, 1 },
        { 3, { 1, 2, 3, 4, 3, 4 }, 0 },
        { 3, { 1, 2, 3, 4, 3, 5, 3 }, 1 },

        // A HIR hit only in Q enters S; only a subsequent hit promotes it.
        { 2, { 1, 2, 2, 1, 3, 1 }, 2 },
        { 2, { 1, 2, 2, 1, 1, 3, 1 }, 4 },
        { 2, { 1, 2, 2, 1, 1, 3, 2 }, 3 },
        { 3, { 1, 2, 3, 3, 1, 4, 1 }, 2 },
        { 3, { 1, 2, 3, 3, 1, 1, 4, 1 }, 4 },
        { 3, { 1, 2, 3, 3, 1, 1, 4, 2 }, 3 },

        // Pruning removes resident HIR from S while preserving its cached value.
        { 2, { 1, 2, 1, 2 }, 2 },
        { 2, { 1, 2, 1, 2, 3, 2 }, 2 },
        { 2, { 1, 2, 1, 2, 3, 1 }, 3 },
        { 2, { 1, 2, 1, 2, 2, 3, 2 }, 4 },
        { 3, { 1, 2, 3, 1, 2, 3, 4, 3 }, 3 },
        { 3, { 1, 2, 3, 1, 2, 3, 3, 4, 3 }, 5 },

        // Pruning discards ghost history; a forgotten key returns as HIR.
        { 2, { 1, 2, 3, 1, 2 }, 1 },
        { 2, { 1, 2, 3, 1, 2, 4, 2 }, 1 },
        { 2, { 1, 2, 3, 1, 2, 4, 1 }, 2 },
        { 2, { 1, 2, 3, 4, 5, 1, 2, 6, 2 }, 1 },
        { 2, { 1, 2, 3, 4, 5, 1, 3, 6, 3 }, 1 },
        { 2, { 1, 2, 3, 4, 5, 1, 4, 6, 4 }, 1 },
        { 2, { 1, 2, 3, 4, 5, 1, 5, 6, 5 }, 2 },
        { 2, { 1, 2, 3, 4, 5, 5, 2, 6, 2 }, 1 },
        { 2, { 1, 2, 3, 4, 5, 5, 3, 6, 3 }, 1 },
        { 2, { 1, 2, 3, 4, 5, 3, 4, 6, 4 }, 0 },

        // Promotion, demotion, eviction, and reentry repeat on the same keys.
        { 2, { 1, 2, 2, 1, 1, 3, 2, 2, 1, 1, 3, 3, 2, 2 }, 8 },
        { 3, { 1, 2, 3, 4, 3, 4, 5, 4, 5, 6, 5, 6, 1, 1, 2, 2, 3, 3 }, 7 },
        { 3, { 1, 2, 3, 3, 1, 1, 2, 2, 4, 4, 3, 3, 5, 5, 1, 1 }, 9 },

        // Pruning stops at the next LIR and preserves more recent HIR history.
        { 3, { 1, 2, 3, 1, 4, 3 }, 1 },
        { 3, { 1, 2, 3, 1, 4, 3, 5, 3 }, 2 },
        { 3, { 1, 2, 3, 1, 4, 3, 5, 2 }, 1 },
        { 3, { 1, 2, 3, 1, 4, 3, 5, 1 }, 2 },

        // Scans preserve LIR pages, even with a working set larger than capacity.
        { 3, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, 0 },
        { 2, { 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3 }, 3 },
        { 3, { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 }, 6 },
        { 3, { 1, 2, 3, 4, 3, 2, 1 }, 2 },
        { 2, { 1, 1, 2, 3, 4, 5, 6, 7, 8, 1, 1 }, 3 },
        { 3, { 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2 }, 4 },
        { 4, { 1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 3 }, 6 },
        { 3, { 1, 2, 1, 2, 3, 4, 5, 3, 4, 5, 3, 4, 5, 1, 2, 1, 2 }, 5 },
        { 4, {
            1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3,
            4
        }, 12 },
        { 5, {
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2,
            1
        }, 10 },

        // Zero, negative, and extreme int keys have no special meaning.
        { 0, { 0, -1, 0, -1 }, 0 },
        { 1, { 0, 0, -1, -1, 0, 0 }, 3 },
        { 1, {
            std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min()
        }, 0 },
        { 3, { -1, 0, 1, -1, 0, 1, -1, 0, 1 }, 6 },
        { 2, { -3, -2, -3, -1, -2, -3, -1, -2 }, 2 },
        { 2, {
            std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0,
            std::numeric_limits<int>::min(), std::numeric_limits<int>::max()
        }, 3 },
        { 2, {
            0, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), 0, 0,
            std::numeric_limits<int>::max()
        }, 3 },
        { 4, {
            std::numeric_limits<int>::min(), -1, 0, std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min(), -1, 0, std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min(), -1, 0, std::numeric_limits<int>::max()
        }, 8 },

        // HIR capacity is max(1, capacity / 100): rounding boundaries.
        { 99, after_scan(99, { 100, 98 }), 1 },
        { 100, after_scan(100, { 101, 99 }), 1 },
        { 101, after_scan(101, { 102, 100 }), 1 },
        { 199, after_scan(199, { 200, 198 }), 1 },
        { 200, after_scan(200, { 201, 198 }), 1 },
        { 200, after_scan(200, { 201, 199 }), 0 },
        { 201, after_scan(201, { 202, 199 }), 1 },
        { 201, after_scan(201, { 202, 200 }), 0 },
        { 299, after_scan(299, { 300, 297 }), 1 },
        { 299, after_scan(299, { 300, 298 }), 0 },
        { 300, after_scan(300, { 301, 297 }), 1 },
        { 300, after_scan(300, { 301, 298 }), 0 },
        { 301, after_scan(301, { 302, 298 }), 1 },
        { 301, after_scan(301, { 302, 299 }), 0 },

        // Several HIR slots: FIFO eviction among untouched resident HIR pages.
        { 300, after_scan(300, { 301, 299 }), 1 },
        { 300, after_scan(300, { 301, 300 }), 1 },
        { 300, after_scan(300, { 301, 302, 299 }), 0 },
        { 300, after_scan(300, { 301, 302, 300 }), 1 },

        // Promote the front, middle, or back of Q; append the demoted LIR.
        { 300, after_scan(300, { 298, 301, 299 }), 1 },
        { 300, after_scan(300, { 298, 301, 1 }), 2 },
        { 300, after_scan(300, { 299, 301, 298 }), 1 },
        { 300, after_scan(300, { 299, 301, 300 }), 2 },
        { 300, after_scan(300, { 300, 301, 298 }), 1 },
        { 300, after_scan(300, { 300, 301, 299 }), 2 },
        { 300, after_scan(300, { 299, 301, 302, 1 }), 2 },
        { 300, after_scan(300, { 299, 301, 302, 303, 1 }), 1 },

        // A Q-only hit refreshes queue order without immediately becoming LIR.
        { 300, after_scan(300, { 298, 299, 300, 1, 301, 2 }), 4 },
        { 300, after_scan(300, { 298, 299, 300, 1, 301, 1 }), 5 },
        { 300, after_scan(300, { 298, 299, 300, 1, 301, 302, 303, 1 }), 4 },
        { 300, after_scan(300, { 298, 299, 300, 2, 301, 1 }), 4 },
        { 300, after_scan(300, { 298, 299, 300, 3, 301, 1 }), 4 },
        { 300, after_scan(300, { 298, 299, 300, 1, 1, 301, 302, 303, 1 }), 6 },

        // Ghost promotion evicts the oldest HIR before demoting a LIR.
        { 300, after_scan(301, { 298, 299 }), 0 },
        { 300, after_scan(301, { 298, 300 }), 1 },
        { 300, after_scan(301, { 298, 302, 303, 1 }), 1 },
        { 300, after_scan(301, { 298, 302, 303, 304, 1 }), 0 },

        // Promotion before the resident HIR partition is completely filled.
        { 200, after_scan(199, { 199, 200, 1 }), 2 },
        { 200, after_scan(199, { 199, 200, 201, 1 }), 1 },
        { 300, after_scan(298, { 298, 299, 300, 1 }), 2 },
        { 300, after_scan(298, { 298, 299, 300, 301, 1 }), 1 },

        // Prune several resident HIR pages at once; their Q order survives.
        { 300, after_scan(300, after_scan(297, { 301, 298 })), 297 },
        { 300, after_scan(300, after_scan(297, { 301, 299 })), 298 },
        { 300, after_scan(300, after_scan(297, { 298, 301, 299 })), 298 },
        { 300, after_scan(300, after_scan(297, { 298, 301, 298 })), 299 },
        { 300, after_scan(300, after_scan(297, { 298, 301, 302, 303, 298 })), 298 },
        { 300, after_scan(300, after_scan(297, { 298, 298, 301, 302, 303, 298 })), 300 },

        // Prune consecutive ghost and resident HIR pages after a LIR hit.
        { 300, after_scan(302, after_scan(297, { 298, 303, 304, 305, 298 })), 297 },
        { 300, after_scan(302, after_scan(297, { 299, 303, 304, 305, 299 })), 297 },
        { 300, after_scan(302, after_scan(297, { 300, 303, 304, 305, 300 })), 298 },
    };

    auto load = [](int key) { return key; };

    for (const auto& test : test_cases)
    {
        SCOPED_TRACE("cache capacity: " + ::testing::PrintToString(test.cache_capacity));
        SCOPED_TRACE("test vector: " + ::testing::PrintToString(test.keys));

        caches::cache_t<int, int> cache(test.cache_capacity);
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
