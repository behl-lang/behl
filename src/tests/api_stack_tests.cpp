#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

class ApiStackTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
    }

    void push_ints(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            behl::push_integer(S, i);
        }
    }
};

TEST_F(ApiStackTest, SetTopGrowsWithNilsAndShrinks)
{
    push_ints(3);
    ASSERT_EQ(behl::get_top(S), 3);

    behl::set_top(S, 5);
    ASSERT_EQ(behl::get_top(S), 5);
    ASSERT_EQ(behl::type(S, 4), behl::Type::kNil);

    behl::set_top(S, 2);
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, 1), 1);

    behl::set_top(S, 0);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, SetTopWithNegativeCountPopsThatMany)
{
    push_ints(5);

    behl::set_top(S, -2);
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -1), 2);

    behl::set_top(S, -1);
    ASSERT_EQ(behl::get_top(S), 2);
}

TEST_F(ApiStackTest, SetTopWithNegativeCountBeyondStackClearsIt)
{
    push_ints(3);

    behl::set_top(S, -10);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, SetTopWithMostNegativeIntegerClearsStack)
{
    push_ints(3);

    behl::set_top(S, std::numeric_limits<int32_t>::min());
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, SetTopWithNegativeCountOnEmptyStackIsHarmless)
{
    behl::set_top(S, -1);
    ASSERT_EQ(behl::get_top(S), 0);

    behl::set_top(S, std::numeric_limits<int32_t>::min());
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, PopRemovesThatManyValues)
{
    push_ints(5);

    behl::pop(S, 2);
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -1), 2);

    behl::pop(S, 3);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, PopOfZeroIsNoOp)
{
    push_ints(3);

    behl::pop(S, 0);
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_F(ApiStackTest, PopBeyondStackClearsIt)
{
    push_ints(3);

    behl::pop(S, 10);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, PopWithNegativeCountIsNoOp)
{
    push_ints(3);

    behl::pop(S, -1);
    ASSERT_EQ(behl::get_top(S), 3);

    behl::pop(S, std::numeric_limits<int32_t>::min());
    ASSERT_EQ(behl::get_top(S), 3);

    ASSERT_EQ(behl::to_integer(S, 0), 0);
    ASSERT_EQ(behl::to_integer(S, 2), 2);
}

TEST_F(ApiStackTest, PopOnEmptyStackIsHarmless)
{
    behl::pop(S, 1);
    ASSERT_EQ(behl::get_top(S), 0);

    behl::pop(S, 100);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, DupCopiesTheAddressedSlot)
{
    push_ints(3);

    behl::dup(S, 0);
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -1), 0);

    behl::dup(S, -2);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_F(ApiStackTest, DupPushesNilForOutOfRangeIndex)
{
    push_ints(2);

    behl::dup(S, 99);
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);

    behl::dup(S, -99);
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(ApiStackTest, DupOnEmptyStackPushesNil)
{
    behl::dup(S, 0);
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(ApiStackTest, RemoveDropsTheAddressedSlotAndShiftsDown)
{
    push_ints(4);

    behl::remove(S, 1);
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, 0), 0);
    ASSERT_EQ(behl::to_integer(S, 1), 2);
    ASSERT_EQ(behl::to_integer(S, 2), 3);
}

TEST_F(ApiStackTest, RemoveAcceptsNegativeIndices)
{
    push_ints(3);

    behl::remove(S, -1);
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -1), 1);

    behl::remove(S, -2);
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(ApiStackTest, RemoveIgnoresOutOfRangeIndex)
{
    push_ints(2);

    behl::remove(S, 99);
    ASSERT_EQ(behl::get_top(S), 2);

    behl::remove(S, -99);
    ASSERT_EQ(behl::get_top(S), 2);

    ASSERT_EQ(behl::to_integer(S, 0), 0);
    ASSERT_EQ(behl::to_integer(S, 1), 1);
}

TEST_F(ApiStackTest, RemoveOnEmptyStackIsHarmless)
{
    behl::remove(S, 0);
    ASSERT_EQ(behl::get_top(S), 0);

    behl::remove(S, -1);
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(ApiStackTest, InsertMovesTopToTheAddressedSlot)
{
    push_ints(3);
    behl::push_integer(S, 99);

    behl::insert(S, 1);
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, 0), 0);
    ASSERT_EQ(behl::to_integer(S, 1), 99);
    ASSERT_EQ(behl::to_integer(S, 2), 1);
    ASSERT_EQ(behl::to_integer(S, 3), 2);
}

TEST_F(ApiStackTest, ToUserdataReturnsNullForNonUserdata)
{
    behl::push_integer(S, 5);
    behl::push_string(S, "text");
    behl::push_nil(S);

    ASSERT_EQ(behl::to_userdata(S, 0), nullptr);
    ASSERT_EQ(behl::to_userdata(S, 1), nullptr);
    ASSERT_EQ(behl::to_userdata(S, 2), nullptr);
}

TEST_F(ApiStackTest, ToUserdataReturnsNullForOutOfRangeIndex)
{
    ASSERT_EQ(behl::to_userdata(S, 0), nullptr);

    behl::push_integer(S, 1);
    ASSERT_EQ(behl::to_userdata(S, 99), nullptr);
    ASSERT_EQ(behl::to_userdata(S, -99), nullptr);
}

TEST_F(ApiStackTest, UserdataGetUidReturnsZeroForNonUserdata)
{
    behl::push_integer(S, 5);
    ASSERT_EQ(behl::userdata_get_uid(S, 0), 0u);
    ASSERT_EQ(behl::userdata_get_uid(S, 99), 0u);
    ASSERT_EQ(behl::userdata_get_uid(S, -99), 0u);
}

TEST_F(ApiStackTest, UserdataRoundTripsThroughTheStack)
{
    constexpr uint32_t kUid = 0xABCD1234u;
    void* data = behl::userdata_new(S, 16, kUid);
    ASSERT_NE(data, nullptr);

    ASSERT_EQ(behl::type(S, -1), behl::Type::kUserdata);
    ASSERT_EQ(behl::userdata_get_uid(S, -1), kUid);
    ASSERT_EQ(behl::to_userdata(S, -1), data);
    ASSERT_EQ(behl::check_userdata(S, -1, kUid), data);
}

TEST_F(ApiStackTest, CheckUserdataRejectsMismatchedUid)
{
    constexpr uint32_t kUid = 0x11111111u;
    behl::userdata_new(S, 8, kUid);

    ASSERT_THROW(behl::check_userdata(S, -1, 0x22222222u), behl::RuntimeError);
}

TEST_F(ApiStackTest, CheckUserdataRejectsNonUserdata)
{
    behl::push_integer(S, 7);
    ASSERT_THROW(behl::check_userdata(S, -1, 1u), behl::TypeError);
}

TEST_F(ApiStackTest, ZeroSizedUserdataIsUsable)
{
    constexpr uint32_t kUid = 0x5A5A5A5Au;
    void* data = behl::userdata_new(S, 0, kUid);

    ASSERT_NE(data, nullptr);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kUserdata);
    ASSERT_EQ(behl::userdata_get_uid(S, -1), kUid);
}
