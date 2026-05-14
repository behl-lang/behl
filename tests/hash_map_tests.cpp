#include "common/hash_map.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

namespace
{
    // Forces every key to probe from slot 0 so the test can place tombstones
    // at known positions without depending on the standard library hash.
    struct AlwaysZeroHash
    {
        size_t operator()(size_t) const noexcept
        {
            return 0;
        }
    };
} // namespace

class HashMapTombstoneTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

// Repro: tombstones are not counted in the load-factor decision, so the table
// can fill with tombstones without ever triggering a rehash. With cap=8 and
// kLoadFactor=0.75 the threshold is 6: after inserting and erasing 7 keys
// every probe slot but one is a tombstone (size=0, tombstones=7). The next
// insert must rehash, otherwise the table operates with pathologically long
// probe chains and only ever cleans up via the fall-through path inside
// insert_or_assign.
TEST_F(HashMapTombstoneTest, TombstonesAreCountedInLoadFactor)
{
    behl::HashMap<size_t, int, AlwaysZeroHash> map;
    map.init(S, 8);

    // Fill positions 0..6. The 7th insert is permitted because needs_rehash
    // is checked before the insert, when size_ is still 6 (6 > 6 is false).
    for (size_t k = 1; k <= 7; ++k)
    {
        map.insert_or_assign(S, k, static_cast<int>(k));
    }
    ASSERT_EQ(map.size(), 7u);
    ASSERT_EQ(map.capacity(), 8u);

    for (size_t k = 1; k <= 7; ++k)
    {
        map.erase(k);
    }
    ASSERT_EQ(map.size(), 0u);
    ASSERT_EQ(map.capacity(), 8u);

    // size_ + tombstones_ = 0 + 7 = 7, which exceeds 0.75 * 8 = 6 — a fixed
    // table must rehash here. The buggy version misses this and keeps cap=8.
    map.insert_or_assign(S, size_t{100}, 100);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.capacity(), 16u);

    map.destroy(S);
}
