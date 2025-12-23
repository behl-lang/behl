#include <behl/behl.hpp>
#include <gtest/gtest.h>

using namespace behl;

class PinningTest : public ::testing::Test
{
protected:
    State* S;

    void SetUp() override
    {
        S = new_state();
        load_stdlib(S, true);

        ASSERT_NE(S, nullptr);
        set_top(S, 0);
    }

    void TearDown() override
    {
        close(S);
    }
};

TEST_F(PinningTest, PinInteger)
{
    push_integer(S, 42);
    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), Type::kInteger);
    ASSERT_EQ(to_integer(S, -1), 42);

    unpin(S, handle);
}

TEST_F(PinningTest, PinString)
{
    push_string(S, "hello world");
    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), Type::kString);
    ASSERT_EQ(to_string(S, -1), "hello world");

    unpin(S, handle);
}

TEST_F(PinningTest, PinTable)
{
    table_new(S);
    push_string(S, "key");
    push_string(S, "value");
    table_rawset(S, -3);

    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), Type::kTable);

    push_string(S, "key");
    table_rawget(S, -2);
    ASSERT_EQ(to_string(S, -1), "value");

    pop(S, 2);
    unpin(S, handle);
}

TEST_F(PinningTest, PinFunction)
{
    constexpr std::string_view code = R"(
        function test() {
            return 123;
        }
        return test;
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));

    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), Type::kClosure);

    ASSERT_NO_THROW(call(S, 0, 1));
    ASSERT_EQ(to_integer(S, -1), 123);

    pop(S, 1);
    unpin(S, handle);
}

TEST_F(PinningTest, MultiplePins)
{
    push_integer(S, 10);
    const auto handle1 = pin(S);

    push_string(S, "test");
    const auto handle2 = pin(S);

    push_number(S, 3.14);
    const auto handle3 = pin(S);

    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle3);
    ASSERT_EQ(to_number(S, -1), 3.14);
    pop(S, 1);

    pinned_push(S, handle2);
    ASSERT_EQ(to_string(S, -1), "test");
    pop(S, 1);

    pinned_push(S, handle1);
    ASSERT_EQ(to_integer(S, -1), 10);
    pop(S, 1);

    unpin(S, handle1);
    unpin(S, handle2);
    unpin(S, handle3);
}

TEST_F(PinningTest, PinSurvivesGC)
{
    table_new(S);
    push_string(S, "data");
    push_string(S, "important");
    table_rawset(S, -3);

    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    gc_collect(S);

    pinned_push(S, handle);
    ASSERT_EQ(type(S, -1), Type::kTable);

    push_string(S, "data");
    table_rawget(S, -2);
    ASSERT_EQ(to_string(S, -1), "important");

    pop(S, 2);
    unpin(S, handle);
}

TEST_F(PinningTest, UnpinAllowsGC)
{
    std::string large_str(1000, 'x');
    push_string(S, large_str);

    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    unpin(S, handle);

    gc_collect(S);
}

TEST_F(PinningTest, PushMultipleTimes)
{
    push_integer(S, 999);
    const auto handle = pin(S);

    pinned_push(S, handle);
    pinned_push(S, handle);
    pinned_push(S, handle);

    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, 0), 999);
    ASSERT_EQ(to_integer(S, 1), 999);
    ASSERT_EQ(to_integer(S, 2), 999);

    pop(S, 3);
    unpin(S, handle);
}

TEST_F(PinningTest, PinNil)
{
    push_nil(S);
    const auto handle = pin(S);

    pinned_push(S, handle);
    ASSERT_EQ(type(S, -1), Type::kNil);

    pop(S, 1);
    unpin(S, handle);
}

TEST_F(PinningTest, PinBoolean)
{
    push_boolean(S, true);
    const auto handle = pin(S);

    pinned_push(S, handle);
    ASSERT_EQ(type(S, -1), Type::kBoolean);
    ASSERT_EQ(to_boolean(S, -1), true);

    pop(S, 1);
    unpin(S, handle);
}

TEST_F(PinningTest, PinClosure)
{
    constexpr std::string_view code = R"(
        let x = 100;
        function make_closure() {
            return function() {
                return x;
            };
        }
        return make_closure();
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));

    const auto handle = pin(S);
    ASSERT_EQ(get_top(S), 0);

    pinned_push(S, handle);
    ASSERT_NO_THROW(call(S, 0, 1));
    ASSERT_EQ(to_integer(S, -1), 100);

    pop(S, 1);
    unpin(S, handle);
}

TEST_F(PinningTest, StressTestManyPins)
{
    constexpr int kNumPins = 1000;

    std::vector<PinHandle> handles;
    handles.reserve(kNumPins);

    for (int i = 0; i < kNumPins; ++i)
    {
        push_integer(S, i);
        handles.push_back(pin(S));
    }

    ASSERT_EQ(get_top(S), 0);

    gc_collect(S);

    for (int i = 0; i < kNumPins; ++i)
    {
        pinned_push(S, handles[static_cast<size_t>(i)]);
        ASSERT_EQ(to_integer(S, -1), i);
        pop(S, 1);
    }

    for (const auto handle : handles)
    {
        unpin(S, handle);
    }
}

TEST_F(PinningTest, PinInCFunction)
{
    static PinHandle callback_handle = PinHandle::kInvalid;

    auto register_callback = [](State* s) -> int {
        if (type(s, -1) != Type::kClosure)
        {
            error(s, "Expected function");
        }
        callback_handle = pin(s);
        return 0;
    };

    auto invoke_callback = [](State* s) -> int {
        if (callback_handle == PinHandle::kInvalid)
        {
            error(s, "No callback registered");
        }
        pinned_push(s, callback_handle);
        call(s, 0, 1);

        EXPECT_EQ(type(s, -1), Type::kInteger);
        EXPECT_EQ(to_integer(s, -1), 42);

        return 1;
    };

    register_function(S, "register_callback", register_callback);
    register_function(S, "invoke_callback", invoke_callback);

    constexpr std::string_view code = R"(
        function my_callback() {
            return 42;
        }
        register_callback(my_callback);
        return invoke_callback();
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    ASSERT_EQ(to_integer(S, -1), 42);

    if (callback_handle != PinHandle::kInvalid)
    {
        unpin(S, callback_handle);
        callback_handle = PinHandle::kInvalid;
    }
}

TEST_F(PinningTest, PinTableWithMetatable)
{
    constexpr std::string_view code = R"(
        let t = {};
        let mt = {
            __tostring = function(self) {
                return "custom";
            }
        };
        setmetatable(t, mt);
        return t;
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));

    const auto handle = pin(S);

    gc_collect(S);

    pinned_push(S, handle);
    ASSERT_EQ(type(S, -1), Type::kTable);
    ASSERT_TRUE(metatable_get(S, -1));
    ASSERT_EQ(type(S, -1), Type::kTable);

    pop(S, 2);
    unpin(S, handle);
}
