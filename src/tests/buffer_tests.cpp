#include "gc/gc.hpp"
#include "gc/gco_buffer.hpp"
#include "state.hpp"
#include "vm/value.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

using namespace behl;

class BufferTest : public ::testing::TestWithParam<bool>
{
protected:
    State* S = nullptr;

    void SetUp() override
    {
        S = new_state();
        ASSERT_NE(S, nullptr);
        S->jit_enabled = GetParam();
        load_stdlib(S);
        set_top(S, 0);
    }

    void TearDown() override
    {
        close(S);
    }

    std::span<std::byte> make_global_buffer(std::string_view name, SysInt len)
    {
        const auto bytes = buffer_new(S, len);
        set_global(S, name);
        return bytes;
    }

    std::string run_expecting_error(std::string_view code)
    {
        if (!behl_test::load_ok(S, code))
        {
            return "<load failed>";
        }
        if (behl::call(S, 0, 0) >= 0)
        {
            return "<no error>";
        }
        return behl_test::error_text(S);
    }

    GCBuffer* buffer_object(int32_t idx)
    {
        return S->stack[static_cast<size_t>(get_top(S) + idx)].get_buffer();
    }
};

TEST_P(BufferTest, NewPushesZeroFilledBuffer)
{
    const auto bytes = buffer_new(S, 16);

    ASSERT_EQ(get_top(S), 1);
    EXPECT_EQ(type(S, -1), Type::kBuffer);
    EXPECT_EQ(value_typename(S, -1), "buffer");
    ASSERT_EQ(bytes.size(), 16u);
    EXPECT_TRUE(std::all_of(bytes.begin(), bytes.end(), [](std::byte b) { return b == std::byte{ 0 }; }));
}

TEST_P(BufferTest, NewWithZeroLengthIsEmpty)
{
    const auto bytes = buffer_new(S, 0);

    EXPECT_EQ(type(S, -1), Type::kBuffer);
    EXPECT_TRUE(bytes.empty());
    EXPECT_EQ(buffer_len(S, -1), 0u);
    EXPECT_TRUE(buffer_get(S, -1).empty());
}

TEST_P(BufferTest, GetReturnsTheSameBytes)
{
    const auto bytes = buffer_new(S, 4);
    bytes[0] = std::byte{ 0x12 };
    bytes[3] = std::byte{ 0xFF };

    const auto again = buffer_get(S, -1);
    ASSERT_EQ(again.size(), 4u);
    EXPECT_EQ(again.data(), bytes.data());
    EXPECT_EQ(again[0], std::byte{ 0x12 });
    EXPECT_EQ(again[3], std::byte{ 0xFF });
    EXPECT_EQ(buffer_len(S, -1), 4u);
}

TEST_P(BufferTest, GetAndLenOnNonBufferAreEmpty)
{
    push_integer(S, 5);
    EXPECT_TRUE(buffer_get(S, -1).empty());
    EXPECT_EQ(buffer_len(S, -1), 0u);

    table_new(S);
    EXPECT_TRUE(buffer_get(S, -1).empty());
    EXPECT_EQ(buffer_len(S, -1), 0u);

    EXPECT_TRUE(buffer_get(S, 50).empty());
    EXPECT_EQ(buffer_len(S, 50), 0u);
}

TEST_P(BufferTest, ResizeGrowKeepsContentsAndZeroFillsTail)
{
    auto bytes = buffer_new(S, 3);
    bytes[0] = std::byte{ 1 };
    bytes[1] = std::byte{ 2 };
    bytes[2] = std::byte{ 3 };

    bytes = buffer_resize(S, -1, 1000);

    ASSERT_EQ(bytes.size(), 1000u);
    EXPECT_EQ(buffer_len(S, -1), 1000u);
    EXPECT_EQ(bytes[0], std::byte{ 1 });
    EXPECT_EQ(bytes[1], std::byte{ 2 });
    EXPECT_EQ(bytes[2], std::byte{ 3 });
    EXPECT_TRUE(std::all_of(bytes.begin() + 3, bytes.end(), [](std::byte b) { return b == std::byte{ 0 }; }));
}

TEST_P(BufferTest, ResizeShrinkKeepsPrefix)
{
    auto bytes = buffer_new(S, 8);
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(i + 10);
    }

    bytes = buffer_resize(S, -1, 3);

    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], std::byte{ 10 });
    EXPECT_EQ(bytes[2], std::byte{ 12 });
}

TEST_P(BufferTest, ResizeToZeroAndBackZeroFills)
{
    auto bytes = buffer_new(S, 4);
    bytes[0] = std::byte{ 7 };

    bytes = buffer_resize(S, -1, 0);
    EXPECT_TRUE(bytes.empty());
    EXPECT_EQ(buffer_len(S, -1), 0u);

    bytes = buffer_resize(S, -1, 4);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_TRUE(std::all_of(bytes.begin(), bytes.end(), [](std::byte b) { return b == std::byte{ 0 }; }));
}

TEST_P(BufferTest, ResizeFromEmptyAllocates)
{
    buffer_new(S, 0);
    const auto bytes = buffer_resize(S, -1, 5);

    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_TRUE(std::all_of(bytes.begin(), bytes.end(), [](std::byte b) { return b == std::byte{ 0 }; }));
}

static int sum_buffer(State* S)
{
    const auto bytes = check_buffer(S, 0);
    Integer sum = 0;
    for (const std::byte b : bytes)
    {
        sum += std::to_integer<Integer>(b);
    }
    push_integer(S, sum);
    return 1;
}

TEST_P(BufferTest, CheckBufferReturnsBytesInsideCall)
{
    push_cfunction(S, sum_buffer);
    const auto bytes = buffer_new(S, 3);
    bytes[0] = std::byte{ 1 };
    bytes[1] = std::byte{ 2 };
    bytes[2] = std::byte{ 250 };

    ASSERT_TRUE(behl_test::call_ok(S, 1, 1));
    EXPECT_EQ(to_integer(S, -1), 253);
}

TEST_P(BufferTest, CheckBufferRaisesBadArgumentForOtherTypes)
{
    push_cfunction(S, sum_buffer);
    push_integer(S, 3);

    ASSERT_TRUE(behl_test::call_fails(S, 1, 1));
    const std::string err = behl_test::error_text(S);
    EXPECT_NE(err.find("TypeError"), std::string::npos) << err;
    EXPECT_NE(err.find("bad argument #1 (expected buffer, got integer)"), std::string::npos) << err;
}

TEST_P(BufferTest, ScriptReadsBytesAsIntegers)
{
    const auto bytes = make_global_buffer("b", 3);
    bytes[0] = std::byte{ 0 };
    bytes[1] = std::byte{ 127 };
    bytes[2] = std::byte{ 255 };

    ASSERT_TRUE(behl_test::load_ok(S, "return b[0], b[1], b[2], typeof(b[2])"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(to_integer(S, -4), 0);
    EXPECT_EQ(to_integer(S, -3), 127);
    EXPECT_EQ(to_integer(S, -2), 255);
    EXPECT_EQ(to_string(S, -1), "integer");
}

TEST_P(BufferTest, ScriptWritesBytes)
{
    const auto bytes = make_global_buffer("b", 4);

    ASSERT_TRUE(behl_test::load_ok(S, "b[0] = 1; b[1] = 200; b[3] = 255"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 0));
    EXPECT_EQ(bytes[0], std::byte{ 1 });
    EXPECT_EQ(bytes[1], std::byte{ 200 });
    EXPECT_EQ(bytes[2], std::byte{ 0 });
    EXPECT_EQ(bytes[3], std::byte{ 255 });
}

TEST_P(BufferTest, ScriptWritesWrapModulo256)
{
    const auto bytes = make_global_buffer("b", 4);

    ASSERT_TRUE(behl_test::load_ok(S, "b[0] = 256; b[1] = 257; b[2] = -1; b[3] = -256"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 0));
    EXPECT_EQ(bytes[0], std::byte{ 0 });
    EXPECT_EQ(bytes[1], std::byte{ 1 });
    EXPECT_EQ(bytes[2], std::byte{ 255 });
    EXPECT_EQ(bytes[3], std::byte{ 0 });
}

TEST_P(BufferTest, ScriptAcceptsIntegralFloats)
{
    const auto bytes = make_global_buffer("b", 4);

    ASSERT_TRUE(behl_test::load_ok(S, "b[2.0] = 9.0; return b[2.0]"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(bytes[2], std::byte{ 9 });
    EXPECT_EQ(to_integer(S, -1), 9);
}

TEST_P(BufferTest, ScriptLengthOperator)
{
    make_global_buffer("b", 37);
    make_global_buffer("e", 0);

    ASSERT_TRUE(behl_test::load_ok(S, "return #b, #e"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_integer(S, -2), 37);
    EXPECT_EQ(to_integer(S, -1), 0);
}

TEST_P(BufferTest, ScriptLoopFillsAndSums)
{
    make_global_buffer("b", 300);

    constexpr std::string_view code = R"(
        for (let i = 0; i < #b; i = i + 1) { b[i] = i }
        let sum = 0
        for (let i = 0; i < #b; i = i + 1) { sum = sum + b[i] }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));

    Integer expected = 0;
    for (Integer i = 0; i < 300; ++i)
    {
        expected += i % 256;
    }
    EXPECT_EQ(to_integer(S, -1), expected);
}

TEST_P(BufferTest, ScriptLastIndexIsValidAndLengthIsOutOfRange)
{
    make_global_buffer("b", 8);

    ASSERT_TRUE(behl_test::load_ok(S, "b[7] = 1; return b[7]"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 1);
    set_top(S, 0);

    const std::string read_err = run_expecting_error("return b[8]");
    EXPECT_NE(read_err.find("RuntimeError"), std::string::npos) << read_err;
    EXPECT_NE(read_err.find("buffer index 8 out of range (length 8)"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("b[8] = 1");
    EXPECT_NE(write_err.find("buffer index 8 out of range (length 8)"), std::string::npos) << write_err;
}

TEST_P(BufferTest, ScriptNegativeIndexIsOutOfRange)
{
    make_global_buffer("b", 8);

    const std::string err = run_expecting_error("return b[-1]");
    EXPECT_NE(err.find("buffer index -1 out of range (length 8)"), std::string::npos) << err;
}

TEST_P(BufferTest, ScriptIndexOnEmptyBufferIsOutOfRange)
{
    make_global_buffer("b", 0);

    const std::string err = run_expecting_error("return b[0]");
    EXPECT_NE(err.find("buffer index 0 out of range (length 0)"), std::string::npos) << err;
}

TEST_P(BufferTest, ScriptNonIntegralIndexRaises)
{
    make_global_buffer("b", 8);

    const std::string read_err = run_expecting_error("return b[1.5]");
    EXPECT_NE(read_err.find("TypeError"), std::string::npos) << read_err;
    EXPECT_NE(read_err.find("number has no integer representation"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("b[1.5] = 1");
    EXPECT_NE(write_err.find("number has no integer representation"), std::string::npos) << write_err;
}

TEST_P(BufferTest, ScriptNonNumericIndexRaises)
{
    make_global_buffer("b", 8);

    const std::string read_err = run_expecting_error("return b.x");
    EXPECT_NE(read_err.find("attempt to index a buffer with a 'string' value"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("b[true] = 1");
    EXPECT_NE(write_err.find("attempt to index a buffer with a 'boolean' value"), std::string::npos) << write_err;
}

TEST_P(BufferTest, ScriptStoringNonNumberRaises)
{
    make_global_buffer("b", 8);

    const std::string err = run_expecting_error("b[0] = \"x\"");
    EXPECT_NE(err.find("attempt to store a 'string' value in a buffer"), std::string::npos) << err;
}

TEST_P(BufferTest, ScriptStoringNonIntegralNumberRaises)
{
    const auto bytes = make_global_buffer("b", 8);

    const std::string err = run_expecting_error("b[0] = 2.5");
    EXPECT_NE(err.find("number has no integer representation"), std::string::npos) << err;
    EXPECT_EQ(bytes[0], std::byte{ 0 });
}

TEST_P(BufferTest, ScriptTypeTostringAndIdentity)
{
    make_global_buffer("b", 2);
    make_global_buffer("c", 2);

    ASSERT_TRUE(behl_test::load_ok(S, "return typeof(b), tostring(b), b == b, b == c"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(to_string(S, -4), "buffer");
    EXPECT_EQ(to_string(S, -3).substr(0, 7), "buffer:");
    EXPECT_TRUE(to_boolean(S, -2));
    EXPECT_FALSE(to_boolean(S, -1));
}

TEST_P(BufferTest, ScriptBufferAsTableKey)
{
    make_global_buffer("b", 2);

    ASSERT_TRUE(behl_test::load_ok(S, "let t = {}; t[b] = 42; return t[b]"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_P(BufferTest, ScriptCallingBufferRaises)
{
    make_global_buffer("b", 2);

    const std::string err = run_expecting_error("b()");
    EXPECT_NE(err.find("attempt to call buffer value"), std::string::npos) << err;
}

TEST_P(BufferTest, ScriptErrorWithBufferValueRoundTrips)
{
    make_global_buffer("b", 2);

    ASSERT_TRUE(behl_test::load_ok(S, "let ok, e = pcall(function() { error(b) }); return ok, e == b"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_FALSE(to_boolean(S, -2));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(BufferTest, BufferReachableFromGlobalSurvivesCollection)
{
    const auto bytes = make_global_buffer("b", 64);
    bytes[63] = std::byte{ 99 };

    gc_collect(S);
    gc_collect(S);

    ASSERT_TRUE(behl_test::load_ok(S, "return b[63], #b"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_integer(S, -2), 99);
    EXPECT_EQ(to_integer(S, -1), 64);
}

TEST_P(BufferTest, UnreachableBuffersAreCollected)
{
    gc_collect(S);
    const size_t before = S->gc.gc_all_objects.count();

    for (int i = 0; i < 50; ++i)
    {
        buffer_new(S, 4096);
        pop(S, 1);
    }
    EXPECT_GE(S->gc.gc_all_objects.count(), before + 50);

    gc_collect(S);
    gc_collect(S);
    EXPECT_LT(S->gc.gc_all_objects.count(), before + 50);
}
TEST_P(BufferTest, SliceSeesRootBytesAndRootSeesSliceWrites)
{
    const auto root_bytes = buffer_new(S, 10);
    for (size_t i = 0; i < root_bytes.size(); ++i)
    {
        root_bytes[i] = static_cast<std::byte>(i);
    }

    GCBuffer* slice = gc_new_buffer_slice(S, buffer_object(-1), 3, 4);
    S->stack.push_back(S, Value(slice));

    const auto slice_bytes = buffer_get(S, -1);
    ASSERT_EQ(slice_bytes.size(), 4u);
    EXPECT_EQ(buffer_len(S, -1), 4u);
    EXPECT_EQ(slice_bytes[0], std::byte{ 3 });
    EXPECT_EQ(slice_bytes[3], std::byte{ 6 });

    slice_bytes[1] = std::byte{ 0xAB };
    EXPECT_EQ(root_bytes[4], std::byte{ 0xAB });
}

TEST_P(BufferTest, SliceOfSlicePointsAtRoot)
{
    buffer_new(S, 10);
    GCBuffer* root = buffer_object(-1);

    GCBuffer* outer = gc_new_buffer_slice(S, root, 2, 6);
    S->stack.push_back(S, Value(outer));
    GCBuffer* inner = gc_new_buffer_slice(S, outer, 1, 3);
    S->stack.push_back(S, Value(inner));

    EXPECT_EQ(inner->owner, root);
    EXPECT_EQ(inner->offset, 3u);
    EXPECT_EQ(inner->len, 3u);
    EXPECT_TRUE(root->is_root());
    EXPECT_FALSE(outer->is_root());
    EXPECT_FALSE(inner->is_root());
}

TEST_P(BufferTest, SliceKeepsRootAliveAfterRootIsDropped)
{
    const auto root_bytes = buffer_new(S, 8);
    root_bytes[5] = std::byte{ 77 };

    GCBuffer* slice = gc_new_buffer_slice(S, buffer_object(-1), 4, 4);
    S->stack.push_back(S, Value(slice));
    remove(S, 0);
    ASSERT_EQ(get_top(S), 1);

    gc_collect(S);
    gc_collect(S);

    const auto slice_bytes = buffer_get(S, -1);
    ASSERT_EQ(slice_bytes.size(), 4u);
    EXPECT_EQ(slice_bytes[1], std::byte{ 77 });
}

TEST_P(BufferTest, SliceSeesRootResize)
{
    buffer_new(S, 8);
    GCBuffer* slice = gc_new_buffer_slice(S, buffer_object(-1), 2, 4);
    S->stack.push_back(S, Value(slice));

    auto root_bytes = buffer_resize(S, 0, 4096);
    root_bytes[3] = std::byte{ 55 };

    const auto slice_bytes = buffer_get(S, -1);
    ASSERT_EQ(slice_bytes.size(), 4u);
    EXPECT_EQ(slice_bytes.data(), root_bytes.data() + 2);
    EXPECT_EQ(slice_bytes[1], std::byte{ 55 });
}

TEST_P(BufferTest, SliceShrinksWhenRootShrinksUnderIt)
{
    buffer_new(S, 10);
    GCBuffer* slice = gc_new_buffer_slice(S, buffer_object(-1), 4, 4);
    S->stack.push_back(S, Value(slice));
    set_global(S, "s");

    buffer_resize(S, 0, 6);
    ASSERT_TRUE(behl_test::load_ok(S, "return #s, s[1]"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_integer(S, -2), 2);
    EXPECT_EQ(to_integer(S, -1), 0);
    set_top(S, 1);

    const std::string err = run_expecting_error("return s[2]");
    EXPECT_NE(err.find("buffer index 2 out of range (length 2)"), std::string::npos) << err;

    buffer_resize(S, 0, 2);
    ASSERT_TRUE(behl_test::load_ok(S, "return #s"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 0);
}

INSTANTIATE_TEST_SUITE_P(Mode, BufferTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
