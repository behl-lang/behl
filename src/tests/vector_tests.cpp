#include "common/vector.hpp"
#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

using namespace behl;

class VectorTest : public ::testing::Test
{
protected:
    State* state = nullptr;

    void SetUp() override
    {
        state = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(state);
    }
};

TEST_F(VectorTest, EraseMiddleElement)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);
    vec.push_back(state, 3);
    vec.push_back(state, 4);
    vec.push_back(state, 5);

    ASSERT_EQ(vec.size(), 5);

    auto it = vec.erase(vec.begin() + 2);

    ASSERT_EQ(vec.size(), 4);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 4);
    EXPECT_EQ(vec[3], 5);
    EXPECT_EQ(*it, 4); // Iterator should point to element after erased one

    vec.destroy(state);
}

TEST_F(VectorTest, EraseFirstElement)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);
    vec.push_back(state, 3);

    auto it = vec.erase(vec.begin());

    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 2);
    EXPECT_EQ(vec[1], 3);
    EXPECT_EQ(*it, 2);

    vec.destroy(state);
}

TEST_F(VectorTest, EraseLastElement)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);
    vec.push_back(state, 3);

    auto it = vec.erase(vec.begin() + 2);

    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(it, vec.end());

    vec.destroy(state);
}

TEST_F(VectorTest, EraseRangeMiddle)
{
    Vector<int> vec;
    vec.init(state, 0);

    for (int i = 1; i <= 10; ++i)
    {
        vec.push_back(state, i);
    }

    auto it = vec.erase(vec.begin() + 3, vec.begin() + 6);

    ASSERT_EQ(vec.size(), 7);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 7);
    EXPECT_EQ(vec[4], 8);
    EXPECT_EQ(vec[5], 9);
    EXPECT_EQ(vec[6], 10);
    EXPECT_EQ(*it, 7);

    vec.destroy(state);
}

TEST_F(VectorTest, EraseRangeAll)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);
    vec.push_back(state, 3);

    auto it = vec.erase(vec.begin(), vec.end());

    ASSERT_EQ(vec.size(), 0);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(it, vec.end());

    vec.destroy(state);
}

TEST_F(VectorTest, EraseRangeEmpty)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);
    vec.push_back(state, 3);

    auto it = vec.erase(vec.begin() + 1, vec.begin() + 1);

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(*it, 2);

    vec.destroy(state);
}

TEST_F(VectorTest, EraseRangeBeginning)
{
    Vector<int> vec;
    vec.init(state, 0);

    for (int i = 1; i <= 5; ++i)
    {
        vec.push_back(state, i);
    }

    auto it = vec.erase(vec.begin(), vec.begin() + 2);

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 3);
    EXPECT_EQ(vec[1], 4);
    EXPECT_EQ(vec[2], 5);
    EXPECT_EQ(*it, 3);

    vec.destroy(state);
}

TEST_F(VectorTest, EraseRangeEnd)
{
    Vector<int> vec;
    vec.init(state, 0);

    for (int i = 1; i <= 5; ++i)
    {
        vec.push_back(state, i);
    }

    auto it = vec.erase(vec.begin() + 3, vec.end());

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(it, vec.end());

    vec.destroy(state);
}

TEST_F(VectorTest, AutoVectorErase)
{
    AutoVector<int> vec(state);

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);

    auto it = vec.erase(vec.begin() + 1);

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 3);
    EXPECT_EQ(vec[2], 4);
    EXPECT_EQ(*it, 3);
}

TEST_F(VectorTest, AutoVectorEraseRange)
{
    AutoVector<int> vec(state);

    for (int i = 1; i <= 6; ++i)
    {
        vec.push_back(i);
    }

    auto it = vec.erase(vec.begin() + 1, vec.begin() + 4);

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 5);
    EXPECT_EQ(vec[2], 6);
    EXPECT_EQ(*it, 5);
}

struct NonTrivial
{
    int value;
    static int construct_count;
    static int destruct_count;

    NonTrivial(int v = 0)
        : value(v)
    {
        ++construct_count;
    }

    NonTrivial(const NonTrivial& other)
        : value(other.value)
    {
        ++construct_count;
    }

    NonTrivial(NonTrivial&& other) noexcept
        : value(other.value)
    {
        ++construct_count;
        other.value = 0;
    }

    ~NonTrivial()
    {
        ++destruct_count;
    }

    NonTrivial& operator=(const NonTrivial& other)
    {
        value = other.value;
        return *this;
    }

    NonTrivial& operator=(NonTrivial&& other) noexcept
    {
        value = other.value;
        other.value = 0;
        return *this;
    }
};

int NonTrivial::construct_count = 0;
int NonTrivial::destruct_count = 0;

TEST_F(VectorTest, EraseNonTrivialType)
{
    NonTrivial::construct_count = 0;
    NonTrivial::destruct_count = 0;

    {
        Vector<NonTrivial> vec;
        vec.init(state, 0);

        vec.push_back(state, NonTrivial(1));
        vec.push_back(state, NonTrivial(2));
        vec.push_back(state, NonTrivial(3));
        vec.push_back(state, NonTrivial(4));

        vec.erase(vec.begin() + 1);

        ASSERT_EQ(vec.size(), 3);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 3);
        EXPECT_EQ(vec[2].value, 4);

        vec.destroy(state);
    }

    EXPECT_EQ(NonTrivial::construct_count, NonTrivial::destruct_count);
}

TEST_F(VectorTest, EraseRangeNonTrivialType)
{
    NonTrivial::construct_count = 0;
    NonTrivial::destruct_count = 0;

    {
        Vector<NonTrivial> vec;
        vec.init(state, 0);

        for (int i = 1; i <= 6; ++i)
        {
            vec.push_back(state, NonTrivial(i));
        }

        vec.erase(vec.begin() + 1, vec.begin() + 4);

        ASSERT_EQ(vec.size(), 3);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 5);
        EXPECT_EQ(vec[2].value, 6);

        vec.destroy(state);
    }

    EXPECT_EQ(NonTrivial::construct_count, NonTrivial::destruct_count);
}

TEST_F(VectorTest, AssignFillsAndReplaces)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 9);
    vec.push_back(state, 8);

    vec.assign(state, 4, 7);

    ASSERT_EQ(vec.size(), 4);
    for (size_t i = 0; i < vec.size(); ++i)
    {
        ASSERT_EQ(vec[i], 7);
    }

    vec.assign(state, 0, 1);
    ASSERT_EQ(vec.size(), 0);
    ASSERT_TRUE(vec.empty());

    vec.destroy(state);
}

TEST_F(VectorTest, AssignGrowsBeyondCapacity)
{
    Vector<int> vec;
    vec.init(state, 2);

    vec.assign(state, 64, 3);

    ASSERT_EQ(vec.size(), 64);
    ASSERT_GE(vec.capacity(), 64u);
    ASSERT_EQ(vec[0], 3);
    ASSERT_EQ(vec[63], 3);

    vec.destroy(state);
}

TEST_F(VectorTest, InsertRangeInMiddle)
{
    Vector<int> vec;
    vec.init(state, 0);

    for (int i = 0; i < 4; ++i)
    {
        vec.push_back(state, i); // 0 1 2 3
    }

    const int extra[] = { 90, 91, 92 };
    auto it = vec.insert(state, vec.begin() + 2, extra, extra + 3);

    ASSERT_EQ(vec.size(), 7);
    ASSERT_EQ(*it, 90);

    const int expected[] = { 0, 1, 90, 91, 92, 2, 3 };
    for (size_t i = 0; i < vec.size(); ++i)
    {
        ASSERT_EQ(vec[i], expected[i]);
    }

    vec.destroy(state);
}

TEST_F(VectorTest, InsertRangeAtBeginAndEnd)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 5);

    const int head[] = { 1, 2 };
    vec.insert(state, vec.begin(), head, head + 2);

    const int tail[] = { 7, 8 };
    vec.insert(state, vec.end(), tail, tail + 2);

    const int expected[] = { 1, 2, 5, 7, 8 };
    ASSERT_EQ(vec.size(), 5);
    for (size_t i = 0; i < vec.size(); ++i)
    {
        ASSERT_EQ(vec[i], expected[i]);
    }

    vec.destroy(state);
}

TEST_F(VectorTest, InsertRangeEmptyIsNoOp)
{
    Vector<int> vec;
    vec.init(state, 0);

    vec.push_back(state, 1);
    vec.push_back(state, 2);

    const int none[] = { 0 };
    auto it = vec.insert(state, vec.begin() + 1, none, none);

    ASSERT_EQ(vec.size(), 2);
    ASSERT_EQ(*it, 2);
    ASSERT_EQ(vec[0], 1);
    ASSERT_EQ(vec[1], 2);

    vec.destroy(state);
}

TEST_F(VectorTest, InsertRangeIntoEmptyVectorGrows)
{
    Vector<int> vec;
    vec.init(state, 0);

    const int values[] = { 4, 5, 6, 7, 8, 9, 10, 11 };
    vec.insert(state, vec.begin(), values, values + 8);

    ASSERT_EQ(vec.size(), 8);
    ASSERT_GE(vec.capacity(), 8u);
    for (size_t i = 0; i < vec.size(); ++i)
    {
        ASSERT_EQ(vec[i], values[i]);
    }

    vec.destroy(state);
}

TEST_F(VectorTest, AutoVectorAssignAndInsertRange)
{
    AutoVector<int> vec(state);

    vec.assign(3, 2);
    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[2], 2);

    const int extra[] = { 40, 41 };
    vec.insert(vec.begin() + 1, extra, extra + 2);

    const int expected[] = { 2, 40, 41, 2, 2 };
    ASSERT_EQ(vec.size(), 5);
    for (size_t i = 0; i < vec.size(); ++i)
    {
        ASSERT_EQ(vec[i], expected[i]);
    }
}
