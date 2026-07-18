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
