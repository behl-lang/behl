#include "common/hash_set.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    struct AlwaysZeroHash
    {
        size_t operator()(size_t) const noexcept
        {
            return 0;
        }
    };
} // namespace

static_assert(sizeof(behl::HashMap<int64_t, behl::detail::EmptyValue>::KeyValue) == sizeof(int64_t));
static_assert(std::is_standard_layout_v<behl::HashSet<int64_t>>);

class HashSetTest : public ::testing::Test
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

TEST_F(HashSetTest, InsertContainsErase)
{
    behl::HashSet<int64_t> set;

    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0u);
    EXPECT_FALSE(set.contains(1));

    set.insert(S, 1);
    set.insert(S, 2);
    set.insert(S, 3);

    EXPECT_FALSE(set.empty());
    EXPECT_EQ(set.size(), 3u);
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_TRUE(set.contains(3));
    EXPECT_FALSE(set.contains(4));

    set.erase(2);
    EXPECT_EQ(set.size(), 2u);
    EXPECT_FALSE(set.contains(2));
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(3));

    set.erase(42);
    EXPECT_EQ(set.size(), 2u);

    set.destroy(S);
}

TEST_F(HashSetTest, DuplicateInsertKeepsOne)
{
    behl::HashSet<int64_t> set;

    set.insert(S, 7);
    set.insert(S, 7);
    set.insert(S, 7);

    EXPECT_EQ(set.size(), 1u);
    EXPECT_TRUE(set.contains(7));

    set.erase(7);
    EXPECT_EQ(set.size(), 0u);
    EXPECT_FALSE(set.contains(7));

    set.destroy(S);
}

TEST_F(HashSetTest, ClearKeepsSetUsable)
{
    behl::HashSet<int64_t> set;

    for (int64_t i = 0; i < 20; ++i)
    {
        set.insert(S, i);
    }
    EXPECT_EQ(set.size(), 20u);

    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_FALSE(set.contains(5));

    set.insert(S, 100);
    EXPECT_EQ(set.size(), 1u);
    EXPECT_TRUE(set.contains(100));

    set.destroy(S);
}

TEST_F(HashSetTest, IterationYieldsAllKeys)
{
    behl::HashSet<int64_t> set;

    for (int64_t i = 1; i <= 8; ++i)
    {
        set.insert(S, i * 10);
    }
    set.erase(30);
    set.erase(70);

    std::vector<int64_t> keys;
    for (const auto& key : set)
    {
        keys.push_back(key);
    }

    EXPECT_EQ(keys.size(), 6u);
    for (const int64_t expected : { 10, 20, 40, 50, 60, 80 })
    {
        EXPECT_NE(std::find(keys.begin(), keys.end(), expected), keys.end()) << "missing key " << expected;
    }

    set.destroy(S);
}

TEST_F(HashSetTest, GrowthAcrossRehash)
{
    behl::HashSet<int64_t> set;

    for (int64_t i = 0; i < 1000; ++i)
    {
        set.insert(S, i);
    }
    EXPECT_EQ(set.size(), 1000u);
    for (int64_t i = 0; i < 1000; ++i)
    {
        EXPECT_TRUE(set.contains(i)) << "missing " << i;
    }
    EXPECT_FALSE(set.contains(1000));

    set.destroy(S);
}

TEST_F(HashSetTest, TombstoneChurnDoesNotHang)
{
    behl::HashSet<size_t, AlwaysZeroHash> set;

    for (int round = 0; round < 100; ++round)
    {
        for (size_t k = 1; k <= 7; ++k)
        {
            set.insert(S, k);
        }
        for (size_t k = 1; k <= 7; ++k)
        {
            set.erase(k);
        }
    }
    EXPECT_TRUE(set.empty());

    set.insert(S, size_t{ 99 });
    EXPECT_TRUE(set.contains(size_t{ 99 }));

    set.destroy(S);
}

TEST_F(HashSetTest, NonTrivialKeys)
{
    behl::HashSet<std::string> set;

    set.insert(S, std::string("alpha"));
    set.insert(S, std::string("beta"));
    set.insert(S, std::string("a-rather-long-string-that-defeats-small-string-optimization"));

    EXPECT_EQ(set.size(), 3u);
    EXPECT_TRUE(set.contains(std::string("alpha")));
    EXPECT_TRUE(set.contains(std::string("a-rather-long-string-that-defeats-small-string-optimization")));
    EXPECT_FALSE(set.contains(std::string("gamma")));

    set.erase(std::string("alpha"));
    EXPECT_FALSE(set.contains(std::string("alpha")));
    EXPECT_EQ(set.size(), 2u);

    set.clear();
    EXPECT_TRUE(set.empty());

    set.destroy(S);
}
