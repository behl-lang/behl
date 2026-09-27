#include "gc/gc.hpp"
#include "gc/gco_buffer.hpp"
#include "state.hpp"
#include "test_helpers.hpp"
#include "vm/value.hpp"

#include <algorithm>
#include <behl/behl.hpp>
#include <cstddef>
#include <gtest/gtest.h>
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

    ::testing::AssertionResult run_lib(std::string_view code, int32_t nresults)
    {
        const std::string full = std::string("const buffer = import(\"buffer\")\n") + std::string(code);
        if (auto loaded = behl_test::load_ok(S, full); !loaded)
        {
            return loaded;
        }
        return behl_test::call_ok(S, 0, nresults);
    }

    std::string lib_error(std::string_view code)
    {
        return run_expecting_error(std::string("const buffer = import(\"buffer\")\n") + std::string(code));
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

TEST_P(BufferTest, LibCreateMakesZeroFilledBuffer)
{
    ASSERT_TRUE(run_lib("let b = buffer.create(5); return typeof(b), #b, b[0], b[4]", 4));
    EXPECT_EQ(to_string(S, -4), "buffer");
    EXPECT_EQ(to_integer(S, -3), 5);
    EXPECT_EQ(to_integer(S, -2), 0);
    EXPECT_EQ(to_integer(S, -1), 0);
}

TEST_P(BufferTest, LibCreateZeroAndNegativeLength)
{
    ASSERT_TRUE(run_lib("return #buffer.create(0)", 1));
    EXPECT_EQ(to_integer(S, -1), 0);
    set_top(S, 0);

    const std::string err = lib_error("buffer.create(-1)");
    EXPECT_NE(err.find("buffer length must not be negative, got -1"), std::string::npos) << err;
}

TEST_P(BufferTest, LibFunctionsRejectNonBuffers)
{
    const std::string err = lib_error("buffer.read_u8(\"abc\", 0)");
    EXPECT_NE(err.find("bad argument #1 (expected buffer, got string)"), std::string::npos) << err;
}

TEST_P(BufferTest, LibWritesAreLittleEndian)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        buffer.write_u32(b, 0, 0x11223344)
        buffer.write_u16(b, 4, 0xAABB)
        return b[0], b[1], b[2], b[3], b[4], b[5]
    )";
    ASSERT_TRUE(run_lib(code, 6));
    EXPECT_EQ(to_integer(S, -6), 0x44);
    EXPECT_EQ(to_integer(S, -5), 0x33);
    EXPECT_EQ(to_integer(S, -4), 0x22);
    EXPECT_EQ(to_integer(S, -3), 0x11);
    EXPECT_EQ(to_integer(S, -2), 0xBB);
    EXPECT_EQ(to_integer(S, -1), 0xAA);
}

TEST_P(BufferTest, LibSignedReadsSignExtendAndUnsignedZeroExtend)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        buffer.write_u8(b, 0, 0xFF)
        buffer.write_u16(b, 2, 0xFFFE)
        buffer.write_u32(b, 4, 0xFFFFFFFD)
        return buffer.read_i8(b, 0), buffer.read_u8(b, 0),
               buffer.read_i16(b, 2), buffer.read_u16(b, 2),
               buffer.read_i32(b, 4), buffer.read_u32(b, 4)
    )";
    ASSERT_TRUE(run_lib(code, 6));
    EXPECT_EQ(to_integer(S, -6), -1);
    EXPECT_EQ(to_integer(S, -5), 255);
    EXPECT_EQ(to_integer(S, -4), -2);
    EXPECT_EQ(to_integer(S, -3), 65534);
    EXPECT_EQ(to_integer(S, -2), -3);
    EXPECT_EQ(to_integer(S, -1), 4294967293);
}

TEST_P(BufferTest, LibSignedAndUnsignedWritesStoreTheSameBits)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        buffer.write_i8(b, 0, -1)
        buffer.write_u8(b, 1, 255)
        buffer.write_i16(b, 2, -2)
        buffer.write_u16(b, 4, 65534)
        return b[0] == b[1], buffer.read_u16(b, 2) == buffer.read_u16(b, 4)
    )";
    ASSERT_TRUE(run_lib(code, 2));
    EXPECT_TRUE(to_boolean(S, -2));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(BufferTest, LibWritesTruncateToWidth)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        buffer.write_u8(b, 0, 0x1FF)
        buffer.write_u16(b, 2, 0x12345)
        buffer.write_i32(b, 4, 0x1FFFFFFFF)
        return buffer.read_u8(b, 0), buffer.read_u16(b, 2), buffer.read_i32(b, 4)
    )";
    ASSERT_TRUE(run_lib(code, 3));
    EXPECT_EQ(to_integer(S, -3), 0xFF);
    EXPECT_EQ(to_integer(S, -2), 0x2345);
    EXPECT_EQ(to_integer(S, -1), -1);
}

TEST_P(BufferTest, LibSixtyFourBitRoundTripsAllBits)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(16)
        buffer.write_i64(b, 0, -2)
        buffer.write_u64(b, 8, 0x7FFFFFFFFFFFFFFF)
        return buffer.read_i64(b, 0), buffer.read_u64(b, 0), buffer.read_i64(b, 8), b[7], b[15]
    )";
    ASSERT_TRUE(run_lib(code, 5));
    EXPECT_EQ(to_integer(S, -5), -2);
    EXPECT_EQ(to_integer(S, -4), -2);
    EXPECT_EQ(to_integer(S, -3), 0x7FFFFFFFFFFFFFFF);
    EXPECT_EQ(to_integer(S, -2), 0xFF);
    EXPECT_EQ(to_integer(S, -1), 0x7F);
}

TEST_P(BufferTest, LibFloatRoundTrips)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(12)
        buffer.write_f64(b, 0, 0.1)
        buffer.write_f32(b, 8, 1.5)
        return buffer.read_f64(b, 0), buffer.read_f32(b, 8), buffer.read_u32(b, 8)
    )";
    ASSERT_TRUE(run_lib(code, 3));
    EXPECT_EQ(to_number(S, -3), 0.1);
    EXPECT_EQ(to_number(S, -2), 1.5);
    EXPECT_EQ(to_integer(S, -1), 0x3FC00000);
}

TEST_P(BufferTest, LibF32RoundsToSinglePrecision)
{
    ASSERT_TRUE(run_lib("let b = buffer.create(4); buffer.write_f32(b, 0, 0.1); return buffer.read_f32(b, 0)", 1));
    EXPECT_EQ(to_number(S, -1), static_cast<double>(0.1f));
}

TEST_P(BufferTest, LibWriteRejectsNonIntegralIntegerValue)
{
    const std::string err = lib_error("let b = buffer.create(4); buffer.write_u8(b, 0, 1.5)");
    EXPECT_NE(err.find("bad argument #3"), std::string::npos) << err;
}

TEST_P(BufferTest, LibReadAtLastValidOffsetAndOnePast)
{
    ASSERT_TRUE(run_lib("let b = buffer.create(8); buffer.write_u32(b, 4, 7); return buffer.read_u32(b, 4)", 1));
    EXPECT_EQ(to_integer(S, -1), 7);
    set_top(S, 0);

    const std::string read_err = lib_error("buffer.read_u32(buffer.create(8), 5)");
    EXPECT_NE(read_err.find("buffer access out of range (offset 5, count 4, length 8)"), std::string::npos) << read_err;

    const std::string write_err = lib_error("buffer.write_u64(buffer.create(8), 1, 0)");
    EXPECT_NE(write_err.find("buffer access out of range (offset 1, count 8, length 8)"), std::string::npos) << write_err;
}

TEST_P(BufferTest, LibNegativeOffsetIsOutOfRange)
{
    const std::string err = lib_error("buffer.read_u8(buffer.create(8), -1)");
    EXPECT_NE(err.find("buffer access out of range (offset -1, count 1, length 8)"), std::string::npos) << err;
}

TEST_P(BufferTest, LibSliceIsAViewIntoTheSource)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(10)
        let s = buffer.slice(b, 4, 3)
        s[0] = 9
        b[6] = 7
        return #s, b[4], s[2], typeof(s)
    )";
    ASSERT_TRUE(run_lib(code, 4));
    EXPECT_EQ(to_integer(S, -4), 3);
    EXPECT_EQ(to_integer(S, -3), 9);
    EXPECT_EQ(to_integer(S, -2), 7);
    EXPECT_EQ(to_string(S, -1), "buffer");
}

TEST_P(BufferTest, LibSliceOfSliceAndTypedAccess)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(16)
        let outer = buffer.slice(b, 4, 8)
        let inner = buffer.slice(outer, 2, 4)
        buffer.write_u32(inner, 0, 0xDEADBEEF)
        return buffer.read_u32(b, 6), #inner
    )";
    ASSERT_TRUE(run_lib(code, 2));
    EXPECT_EQ(to_integer(S, -2), 0xDEADBEEF);
    EXPECT_EQ(to_integer(S, -1), 4);
}

TEST_P(BufferTest, LibSliceBoundsAreChecked)
{
    ASSERT_TRUE(run_lib("return #buffer.slice(buffer.create(8), 8, 0), #buffer.slice(buffer.create(8), 0, 8)", 2));
    EXPECT_EQ(to_integer(S, -2), 0);
    EXPECT_EQ(to_integer(S, -1), 8);
    set_top(S, 0);

    const std::string err = lib_error("buffer.slice(buffer.create(8), 5, 4)");
    EXPECT_NE(err.find("buffer access out of range (offset 5, count 4, length 8)"), std::string::npos) << err;
}

TEST_P(BufferTest, LibResizeGrowsAndSliceSeesIt)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(4)
        b[3] = 5
        let s = buffer.slice(b, 2, 2)
        buffer.resize(b, 64)
        b[2] = 8
        return #b, b[3], b[63], s[0], s[1]
    )";
    ASSERT_TRUE(run_lib(code, 5));
    EXPECT_EQ(to_integer(S, -5), 64);
    EXPECT_EQ(to_integer(S, -4), 5);
    EXPECT_EQ(to_integer(S, -3), 0);
    EXPECT_EQ(to_integer(S, -2), 8);
    EXPECT_EQ(to_integer(S, -1), 5);
}

TEST_P(BufferTest, LibResizeRejectsSlicesAndNegativeLength)
{
    const std::string slice_err = lib_error("let b = buffer.create(4); buffer.resize(buffer.slice(b, 0, 2), 8)");
    EXPECT_NE(slice_err.find("a buffer slice can not be resized"), std::string::npos) << slice_err;

    const std::string neg_err = lib_error("buffer.resize(buffer.create(4), -3)");
    EXPECT_NE(neg_err.find("buffer length must not be negative, got -3"), std::string::npos) << neg_err;
}

TEST_P(BufferTest, LibStringConversions)
{
    constexpr std::string_view code = R"(
        let b = buffer.from_string("hello")
        return #b, b[1], buffer.to_string(b), buffer.to_string(b, 1), buffer.to_string(b, 1, 3), buffer.to_string(b, 5)
    )";
    ASSERT_TRUE(run_lib(code, 6));
    EXPECT_EQ(to_integer(S, -6), 5);
    EXPECT_EQ(to_integer(S, -5), 'e');
    EXPECT_EQ(to_string(S, -4), "hello");
    EXPECT_EQ(to_string(S, -3), "ello");
    EXPECT_EQ(to_string(S, -2), "ell");
    EXPECT_EQ(to_string(S, -1), "");
}

TEST_P(BufferTest, LibStringConversionKeepsEmbeddedZeros)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(3)
        b[0] = 65
        b[2] = 66
        let s = buffer.to_string(b)
        return #s, #buffer.from_string("")
    )";
    ASSERT_TRUE(run_lib(code, 2));
    EXPECT_EQ(to_integer(S, -2), 3);
    EXPECT_EQ(to_integer(S, -1), 0);
}

TEST_P(BufferTest, LibToStringRangeIsChecked)
{
    const std::string err = lib_error("buffer.to_string(buffer.create(4), 2, 3)");
    EXPECT_NE(err.find("buffer access out of range (offset 2, count 3, length 4)"), std::string::npos) << err;
}

TEST_P(BufferTest, LibCopyBetweenBuffersWithDefaults)
{
    constexpr std::string_view code = R"(
        let src = buffer.from_string("abc")
        let dst = buffer.create(6)
        buffer.copy(dst, 2, src)
        buffer.copy(dst, 0, src, 1, 1)
        return dst[0], dst[1], dst[2], dst[4]
    )";
    ASSERT_TRUE(run_lib(code, 4));
    EXPECT_EQ(to_integer(S, -4), 'b');
    EXPECT_EQ(to_integer(S, -3), 0);
    EXPECT_EQ(to_integer(S, -2), 'a');
    EXPECT_EQ(to_integer(S, -1), 'c');
}

TEST_P(BufferTest, LibCopyHandlesOverlapInBothDirections)
{
    constexpr std::string_view code = R"(
        let a = buffer.from_string("abcdef")
        buffer.copy(a, 2, a, 0, 4)
        let b = buffer.from_string("abcdef")
        buffer.copy(b, 0, b, 2, 4)
        return buffer.to_string(a), buffer.to_string(b)
    )";
    ASSERT_TRUE(run_lib(code, 2));
    EXPECT_EQ(to_string(S, -2), "ababcd");
    EXPECT_EQ(to_string(S, -1), "cdefef");
}

TEST_P(BufferTest, LibCopyRangesAreChecked)
{
    const std::string src_err = lib_error("buffer.copy(buffer.create(8), 0, buffer.create(4), 2, 3)");
    EXPECT_NE(src_err.find("buffer access out of range (offset 2, count 3, length 4)"), std::string::npos) << src_err;

    const std::string dst_err = lib_error("buffer.copy(buffer.create(4), 2, buffer.create(8))");
    EXPECT_NE(dst_err.find("buffer access out of range (offset 2, count 8, length 4)"), std::string::npos) << dst_err;
}

TEST_P(BufferTest, LibFillWithDefaultAndExplicitCount)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(6)
        buffer.fill(b, 2, 7)
        buffer.fill(b, 0, 0x141, 1)
        return b[0], b[1], b[2], b[5]
    )";
    ASSERT_TRUE(run_lib(code, 4));
    EXPECT_EQ(to_integer(S, -4), 0x41);
    EXPECT_EQ(to_integer(S, -3), 0);
    EXPECT_EQ(to_integer(S, -2), 7);
    EXPECT_EQ(to_integer(S, -1), 7);
}

TEST_P(BufferTest, LibFillRangeIsChecked)
{
    const std::string err = lib_error("buffer.fill(buffer.create(4), 1, 0, 4)");
    EXPECT_NE(err.find("buffer access out of range (offset 1, count 4, length 4)"), std::string::npos) << err;
}

TEST_P(BufferTest, IndicesBeyondThirtyTwoBitsAreOutOfRange)
{
    const auto bytes = make_global_buffer("b", 4);
    bytes[0] = std::byte{ 7 };

    const std::string read_err = run_expecting_error("return b[4294967296]");
    EXPECT_NE(read_err.find("buffer index 4294967296 out of range (length 4)"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("b[4294967296] = 9");
    EXPECT_NE(write_err.find("buffer index 4294967296 out of range (length 4)"), std::string::npos) << write_err;
    EXPECT_EQ(bytes[0], std::byte{ 7 });

    const std::string lib_read_err = lib_error("buffer.read_u8(b, 4294967296)");
    EXPECT_NE(lib_read_err.find("buffer access out of range (offset 4294967296, count 1, length 4)"), std::string::npos)
        << lib_read_err;

    const std::string lib_fill_err = lib_error("buffer.fill(b, 0, 1, 4294967297)");
    EXPECT_NE(lib_fill_err.find("buffer access out of range (offset 0, count 4294967297, length 4)"), std::string::npos)
        << lib_fill_err;
    EXPECT_EQ(bytes[1], std::byte{ 0 });
}

TEST_P(BufferTest, CreateRejectsLengthsThatDoNotFitSysInt)
{
    if constexpr (sizeof(SysInt) >= 8)
    {
        GTEST_SKIP() << "every non-negative Integer fits a 64-bit SysInt";
    }
    else
    {
        const std::string err = lib_error("buffer.create(4294967296)");
        EXPECT_NE(err.find("buffer length 4294967296 is too large"), std::string::npos) << err;
    }
}

static bool gc_owns_object(State* S, const GCObject* target)
{
    for (GCObject* obj = S->gc.gc_all_objects.head(); obj != nullptr; obj = obj->get_header().next)
    {
        if (obj == target)
        {
            return true;
        }
    }
    return false;
}

static void finish_gc_cycle(State* S)
{
    for (int i = 0; i < 100000 && S->gc.gc_phase != GCPhase::kIdle; ++i)
    {
        gc_step(S);
    }
}

static void run_incremental_cycles(State* S, int cycles)
{
    for (int c = 0; c < cycles; ++c)
    {
        S->gc.gc_debt = int64_t{ 1 } << 30;
        gc_step(S);
        finish_gc_cycle(S);
    }
}

TEST_P(BufferTest, SliceKeepsRootAliveThroughIncrementalCycles)
{
    constexpr std::string_view setup = R"(
        function make() {
            let b = buffer.create(64)
            for (let i = 0; i < 64; i = i + 1) { b[i] = i }
            return buffer.slice(b, 8, 16)
        }
        held = make()
        for (let i = 0; i < 5000; i = i + 1) { let t = { i, i + 1 }; let s = "x" + tostring(i) }
    )";
    ASSERT_TRUE(run_lib(setup, 0));

    run_incremental_cycles(S, 3);

    constexpr std::string_view check = R"(
        let sum = 0
        for (let i = 0; i < #held; i = i + 1) { sum = sum + held[i] }
        held[15] = 200
        return #held, sum, held[0], held[15]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, check));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(to_integer(S, -4), 16);
    EXPECT_EQ(to_integer(S, -3), 248);
    EXPECT_EQ(to_integer(S, -2), 8);
    EXPECT_EQ(to_integer(S, -1), 200);
}

TEST_P(BufferTest, SliceCreatedMidMarkKeepsWhiteRootAlive)
{
    constexpr std::string_view setup = R"(
        let inner = { buf = buffer.create(32) }
        let node = inner
        for (let i = 0; i < 300; i = i + 1) { node = { next = node } }
        holder = node
    )";
    ASSERT_TRUE(run_lib(setup, 0));
    set_top(S, 0);

    get_global(S, "holder");
    for (int i = 0; i < 300; ++i)
    {
        table_getfield(S, -1, "next");
        remove(S, -2);
    }
    GCTable* inner = S->stack[static_cast<size_t>(get_top(S) - 1)].get_table();
    table_getfield(S, -1, "buf");
    ASSERT_EQ(type(S, -1), Type::kBuffer);
    GCBuffer* root = buffer_object(-1);
    set_top(S, 0);

    finish_gc_cycle(S);
    ASSERT_EQ(S->gc.gc_phase, GCPhase::kIdle);
    S->gc.gc_debt = int64_t{ 1 } << 30;
    gc_step(S);
    ASSERT_EQ(S->gc.gc_phase, GCPhase::kMark);
    ASSERT_EQ(root->header.color, GCColor::kWhite) << "root was reached before the slice was taken";

    GCBuffer* slice = gc_new_buffer_slice(S, root, 4, 8);
    S->stack.push_back(S, Value(slice));
    EXPECT_NE(root->header.color, GCColor::kWhite) << "slice creation did not grey its white root";

    S->stack.push_back(S, Value(inner));
    push_nil(S);
    table_setfield(S, 1, "buf");
    pop(S, 1);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, 0), Type::kBuffer);

    finish_gc_cycle(S);

    ASSERT_TRUE(gc_owns_object(S, root)) << "root was swept while a slice still referenced it";
    EXPECT_EQ(buffer_len(S, 0), 8u);
}

TEST_P(BufferTest, RootAndSlicesUnreachableTogetherAreCollected)
{
    gc_collect(S);
    const size_t before = S->gc.gc_all_objects.count();

    buffer_new(S, 128);
    GCBuffer* root = buffer_object(-1);
    GCBuffer* a = gc_new_buffer_slice(S, root, 0, 64);
    S->stack.push_back(S, Value(a));
    GCBuffer* b = gc_new_buffer_slice(S, a, 8, 16);
    S->stack.push_back(S, Value(b));
    GCBuffer* c = gc_new_buffer_slice(S, root, 100, 28);
    S->stack.push_back(S, Value(c));
    ASSERT_EQ(S->gc.gc_all_objects.count(), before + 4);

    set_top(S, 0);
    gc_collect(S);
    gc_collect(S);
    EXPECT_LE(S->gc.gc_all_objects.count(), before);
    EXPECT_FALSE(gc_owns_object(S, root));

    set_top(S, 0);
    buffer_new(S, 16);
    GCBuffer* root2 = buffer_object(-1);
    S->stack.push_back(S, Value(gc_new_buffer_slice(S, root2, 0, 4)));
    set_top(S, 0);
    run_incremental_cycles(S, 2);
    EXPECT_FALSE(gc_owns_object(S, root2));
}

TEST_P(BufferTest, SliceSurvivesRootResizeToZeroAndBack)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        let s = buffer.slice(b, 2, 4)
        buffer.resize(b, 0)
        let empty_len = #s
        let ok, err = pcall(function() { return s[0] })
        buffer.resize(b, 8)
        s[3] = 9
        return empty_len, ok, #s, s[0], b[5]
    )";
    ASSERT_TRUE(run_lib(code, 5));
    EXPECT_EQ(to_integer(S, -5), 0);
    EXPECT_FALSE(to_boolean(S, -4));
    EXPECT_EQ(to_integer(S, -3), 4);
    EXPECT_EQ(to_integer(S, -2), 0);
    EXPECT_EQ(to_integer(S, -1), 9);
}

TEST_P(BufferTest, LibOperationsOnSliceOfShrunkRootStayInBounds)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(8)
        let s = buffer.slice(b, 4, 4)
        buffer.resize(b, 5)
        buffer.fill(s, 0, 7)
        return #s, b[4], buffer.to_string(s), pcall(buffer.read_u32, s, 0)
    )";
    ASSERT_TRUE(run_lib(code, 5));
    EXPECT_EQ(to_integer(S, -5), 1);
    EXPECT_EQ(to_integer(S, -4), 7);
    EXPECT_EQ(to_string(S, -3), std::string_view("\x07", 1));
    EXPECT_FALSE(to_boolean(S, -2));
    const std::string err = behl_test::error_text(S);
    EXPECT_NE(err.find("buffer access out of range (offset 0, count 4, length 1)"), std::string::npos) << err;
    set_top(S, 0);

    const std::string copy_err = lib_error(R"(
        let b = buffer.create(8)
        let s = buffer.slice(b, 4, 4)
        buffer.resize(b, 4)
        buffer.copy(s, 0, buffer.create(2))
    )");
    EXPECT_NE(copy_err.find("buffer access out of range (offset 0, count 2, length 0)"), std::string::npos) << copy_err;
}

static constexpr bool kNativeBufferAccess = BEHL_JIT_X86_64 || BEHL_JIT_AARCH64;

static uint64_t total_helper_calls(State* S)
{
#if BEHL_JIT_SUPPORTED
    uint64_t total = 0;
    for (const uint64_t count : S->jit_stats.helper_calls)
    {
        total += count;
    }
    return total;
#else
    (void)S;
    return 0;
#endif
}

TEST_P(BufferTest, HotLoopRegisterKeyReadsAndWritesStayNative)
{
    const auto bytes = make_global_buffer("b", 1024);

    constexpr std::string_view code = R"(
        let buf = b
        for (let i = 0; i < 1024; i = i + 1) { buf[i] = i * 3 }
        let sum = 0
        for (let i = 0; i < 1024; i = i + 1) { sum = sum + buf[i] }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const uint64_t before = total_helper_calls(S);
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    const uint64_t helpers = total_helper_calls(S) - before;

    Integer expected = 0;
    for (Integer i = 0; i < 1024; ++i)
    {
        expected += (i * 3) % 256;
        EXPECT_EQ(bytes[static_cast<size_t>(i)], static_cast<std::byte>((i * 3) % 256)) << i;
    }
    EXPECT_EQ(to_integer(S, -1), expected);
    if (GetParam() && kNativeBufferAccess)
    {
        EXPECT_LT(helpers, 64u) << "buffer element access fell back to the generic field helpers";
    }
}

TEST_P(BufferTest, HotLoopImmediateKeyReadsAndWritesStayNative)
{
    const auto bytes = make_global_buffer("b", 8);

    constexpr std::string_view code = R"(
        let buf = b
        let sum = 0
        for (let i = 0; i < 1000; i = i + 1) {
            buf[3] = i
            buf[0] = buf[3] + 1
            sum = sum + buf[0] + buf[7]
        }
        return sum, buf[0], buf[3]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const uint64_t before = total_helper_calls(S);
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    const uint64_t helpers = total_helper_calls(S) - before;

    Integer expected = 0;
    for (Integer i = 0; i < 1000; ++i)
    {
        expected += ((i % 256) + 1) % 256;
    }
    EXPECT_EQ(to_integer(S, -3), expected);
    EXPECT_EQ(to_integer(S, -2), (999 % 256 + 1) % 256);
    EXPECT_EQ(to_integer(S, -1), 999 % 256);
    EXPECT_EQ(bytes[3], static_cast<std::byte>(999 % 256));
    if (GetParam() && kNativeBufferAccess)
    {
        EXPECT_LT(helpers, 64u) << "buffer element access fell back to the generic field helpers";
    }
}

TEST_P(BufferTest, HotLoopOnSliceUsesTheSliceOffset)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(32)
        for (let i = 0; i < 32; i = i + 1) { b[i] = i }
        let s = buffer.slice(b, 8, 16)
        let sum = 0
        for (let i = 0; i < 16; i = i + 1) { sum = sum + s[i]; s[i] = 100 + i }
        return sum, b[8], b[23], b[24], s[15], b[7]
    )";
    ASSERT_TRUE(run_lib(code, 6));
    EXPECT_EQ(to_integer(S, -6), 248);
    EXPECT_EQ(to_integer(S, -5), 100);
    EXPECT_EQ(to_integer(S, -4), 115);
    EXPECT_EQ(to_integer(S, -3), 24);
    EXPECT_EQ(to_integer(S, -2), 115);
    EXPECT_EQ(to_integer(S, -1), 7);
}

TEST_P(BufferTest, HotLoopOnSliceOfShrunkRootClampsTheLength)
{
    constexpr std::string_view code = R"(
        let b = buffer.create(16)
        s = buffer.slice(b, 4, 8)
        buffer.resize(b, 10)
        let n = 0
        for (let i = 0; i < 6; i = i + 1) { s[i] = i + 1; n = n + s[i] }
        return #s, n, b[9]
    )";
    ASSERT_TRUE(run_lib(code, 3));
    EXPECT_EQ(to_integer(S, -3), 6);
    EXPECT_EQ(to_integer(S, -2), 21);
    EXPECT_EQ(to_integer(S, -1), 6);
    set_top(S, 0);

    const std::string read_err = run_expecting_error("let t = s; for (let i = 0; i < 8; i = i + 1) { let x = t[i] }");
    EXPECT_NE(read_err.find("buffer index 6 out of range (length 6)"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("let t = s; for (let i = 0; i < 8; i = i + 1) { t[i] = 1 }");
    EXPECT_NE(write_err.find("buffer index 6 out of range (length 6)"), std::string::npos) << write_err;

    const std::string imm_err = run_expecting_error("let t = s; return t[6]");
    EXPECT_NE(imm_err.find("buffer index 6 out of range (length 6)"), std::string::npos) << imm_err;

    const std::string gone_err = lib_error(R"(
        let b = buffer.create(16)
        let t = buffer.slice(b, 4, 8)
        buffer.resize(b, 3)
        for (let i = 0; i < 4; i = i + 1) { t[i] = 1 }
    )");
    EXPECT_NE(gone_err.find("buffer index 0 out of range (length 0)"), std::string::npos) << gone_err;
}

TEST_P(BufferTest, HotLoopNegativeIndexIsOutOfRange)
{
    make_global_buffer("b", 8);

    const std::string read_err = run_expecting_error("let t = b; for (let i = 3; i > -5; i = i - 1) { let x = t[i] }");
    EXPECT_NE(read_err.find("buffer index -1 out of range (length 8)"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("let t = b; for (let i = 3; i > -5; i = i - 1) { t[i] = 1 }");
    EXPECT_NE(write_err.find("buffer index -1 out of range (length 8)"), std::string::npos) << write_err;
}

TEST_P(BufferTest, HotLoopImmediateKeyPastTheEndIsOutOfRange)
{
    make_global_buffer("b", 8);

    const std::string read_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { let x = t[9] }");
    EXPECT_NE(read_err.find("buffer index 9 out of range (length 8)"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { t[8] = i }");
    EXPECT_NE(write_err.find("buffer index 8 out of range (length 8)"), std::string::npos) << write_err;
}

TEST_P(BufferTest, HotLoopFloatKeys)
{
    const auto bytes = make_global_buffer("b", 8);

    constexpr std::string_view code = R"(
        let t = b
        let r = 0
        for (let i = 0; i < 4; i = i + 1) { let k = 2.0; t[k] = 5 + i; r = t[k] }
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 8);
    EXPECT_EQ(bytes[2], std::byte{ 8 });
    set_top(S, 0);

    const std::string read_err = run_expecting_error(
        "let t = b; for (let i = 0; i < 4; i = i + 1) { let k = 1.5; let x = t[k] }");
    EXPECT_NE(read_err.find("number has no integer representation"), std::string::npos) << read_err;

    const std::string write_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { let k = 1.5; t[k] = 1 }");
    EXPECT_NE(write_err.find("number has no integer representation"), std::string::npos) << write_err;
}

TEST_P(BufferTest, HotLoopStoredValuesWrapAndConvert)
{
    const auto bytes = make_global_buffer("b", 8);

    constexpr std::string_view code = R"(
        let t = b
        for (let i = 0; i < 4; i = i + 1) {
            let v = 256 + i
            t[0] = v
            v = -1 - i
            t[1] = v
            let f = 3.0 + i
            t[2] = f
            t[3] = 511
        }
        return t[0], t[1], t[2], t[3]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(to_integer(S, -4), 3);
    EXPECT_EQ(to_integer(S, -3), 252);
    EXPECT_EQ(to_integer(S, -2), 6);
    EXPECT_EQ(to_integer(S, -1), 255);
    EXPECT_EQ(bytes[1], std::byte{ 252 });
    set_top(S, 0);

    const std::string frac_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { let f = 2.5; t[i] = f }");
    EXPECT_NE(frac_err.find("number has no integer representation"), std::string::npos) << frac_err;
    EXPECT_EQ(bytes[0], std::byte{ 3 });

    const std::string str_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { let v = \"x\"; t[i] = v }");
    EXPECT_NE(str_err.find("attempt to store a 'string' value in a buffer"), std::string::npos) << str_err;

    const std::string imm_err = run_expecting_error("let t = b; for (let i = 0; i < 4; i = i + 1) { t[5] = \"x\" }");
    EXPECT_NE(imm_err.find("attempt to store a 'string' value in a buffer"), std::string::npos) << imm_err;
}

TEST_P(BufferTest, TablesStillWorkThroughTheSharedIntegerKeyPath)
{
    make_global_buffer("b", 4);

    constexpr std::string_view code = R"(
        let t = {1, 2, 3, 4}
        let buf = b
        let sum = 0
        for (let i = 0; i < 4; i = i + 1) {
            let c = t
            if (i % 2 == 1) { c = buf }
            c[i] = i + 10
            sum = sum + c[i] + c[0]
        }
        return sum, t[0], t[2], buf[1], buf[3]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(to_integer(S, -5), 10 + 10 + 11 + 0 + 12 + 10 + 13 + 0);
    EXPECT_EQ(to_integer(S, -4), 10);
    EXPECT_EQ(to_integer(S, -3), 12);
    EXPECT_EQ(to_integer(S, -2), 11);
    EXPECT_EQ(to_integer(S, -1), 13);
}

INSTANTIATE_TEST_SUITE_P(Mode, BufferTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
