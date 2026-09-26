#include "state.hpp"

#include <algorithm>
#include <behl/behl.hpp>
#include <cctype>
#include <filesystem>
#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <string>
#include <string_view>

using namespace std::string_view_literals;

class FsTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;
    std::string dir;

    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string("behl_fs_") + info->test_suite_name() + "_" + info->name();
        for (char& c : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)))
            {
                c = '_';
            }
        }
        dir = (std::filesystem::temp_directory_path() / name).generic_string();

        wipe_dir();

        S = new_fs_state();
        run("return fs.mkdir(dir);", 1);
        ASSERT_TRUE(behl::to_boolean(S, -1));
        behl::set_top(S, 0);
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
        EXPECT_FALSE(wipe_dir());
    }

    behl::State* new_fs_state() const
    {
        behl::State* state = behl::new_state();
        state->jit_enabled = GetParam();
        behl::load_stdlib(state);
        behl::load_lib_fs(state);
        return state;
    }

    std::string prelude() const
    {
        return "const fs = import(\"fs\");\nconst string = import(\"string\");\nlet dir = \"" + dir + "\";\n";
    }

    bool wipe_dir() const
    {
        behl::State* state = new_fs_state();
        const std::string code = prelude() + R"(
            function wipe(p) {
                if (fs.is_dir(p)) {
                    let entries = fs.list(p);
                    for (let i = 0; i < #entries; i++) {
                        wipe(fs.join(p, entries[i]));
                    }
                    fs.rmdir(p);
                } else {
                    if (fs.exists(p)) {
                        fs.remove(p);
                    }
                }
            }
            wipe(dir);
            return fs.exists(dir);
        )";
        bool still_exists = true;
        if (behl::load_string(state, code) == 0 && behl::call(state, 0, 1) >= 0)
        {
            still_exists = behl::to_boolean(state, -1);
        }
        behl::close(state);
        return still_exists;
    }

    void run(std::string_view body, int results)
    {
        const std::string code = prelude() + std::string(body);
        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, results));
    }

    static bool is_meaningful_error(behl::State* state, int idx)
    {
        if (!behl::is_string(state, idx))
        {
            return false;
        }
        std::string message(behl::to_string(state, idx));
        if (message.empty())
        {
            return false;
        }
        std::transform(message.begin(), message.end(), message.begin(),
            [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        return message.find("success") == std::string::npos;
    }
};

TEST_P(FsTest, SeekCurMovesByOffsetOnce)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let w = fs.open(p, "w");
        w:write("0123456789");
        w:close();
        let f = fs.open(p, "r");
        f:read(3);
        let pos = f:seek("cur", 1);
        let c = f:read(1);
        f:close();
        return pos, c;
    )",
        2);
    EXPECT_EQ(behl::to_integer(S, -2), 4);
    EXPECT_EQ(behl::to_string(S, -1), "4");
}

TEST_P(FsTest, SeekCurZeroReportsCurrentPosition)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let w = fs.open(p, "w");
        w:write("0123456789");
        w:close();
        let f = fs.open(p, "r");
        f:read(6);
        let pos = f:seek("cur", 0);
        let c = f:read(1);
        f:close();
        return pos, c;
    )",
        2);
    EXPECT_EQ(behl::to_integer(S, -2), 6);
    EXPECT_EQ(behl::to_string(S, -1), "6");
}

TEST_P(FsTest, SeekAfterReadingToEofSucceeds)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let w = fs.open(p, "w");
        w:write("0123456789");
        w:close();
        let f = fs.open(p, "r");
        f:read(100);
        let set0 = f:seek("set", 0);
        let c = f:read(2);
        let endpos = f:seek("end", 0);
        f:close();
        return set0, c, endpos;
    )",
        3);
    EXPECT_EQ(behl::to_integer(S, -3), 0);
    EXPECT_EQ(behl::to_string(S, -2), "01");
    EXPECT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(FsTest, SeekSetAndEndBeforeEof)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let w = fs.open(p, "w");
        w:write("0123456789");
        w:close();
        let f = fs.open(p, "r");
        let s1 = f:seek("set", 1);
        let a = f:read(2);
        let e0 = f:seek("end", 0);
        let e2 = f:seek("end", -2);
        let b = f:read(2);
        f:close();
        return s1, a, e0, e2, b;
    )",
        5);
    EXPECT_EQ(behl::to_integer(S, -5), 1);
    EXPECT_EQ(behl::to_string(S, -4), "12");
    EXPECT_EQ(behl::to_integer(S, -3), 10);
    EXPECT_EQ(behl::to_integer(S, -2), 8);
    EXPECT_EQ(behl::to_string(S, -1), "89");
}

TEST_P(FsTest, ReadMissingFileReportsMeaningfulError)
{
    run(R"(
        return fs.read(fs.join(dir, "missing.txt"));
    )",
        2);
    EXPECT_EQ(behl::type(S, -2), behl::Type::kBoolean);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1)) << behl::to_string(S, -1);
}

TEST_P(FsTest, ListMissingDirectoryReportsMeaningfulError)
{
    run(R"(
        return fs.list(fs.join(dir, "missing"));
    )",
        2);
    EXPECT_EQ(behl::type(S, -2), behl::Type::kBoolean);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1)) << behl::to_string(S, -1);
}

TEST_P(FsTest, OpenMissingFileReportsMeaningfulError)
{
    run(R"(
        return fs.open(fs.join(dir, "missing.txt"), "r");
    )",
        2);
    EXPECT_EQ(behl::type(S, -2), behl::Type::kBoolean);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1)) << behl::to_string(S, -1);
}

TEST_P(FsTest, ReadReturnsWholeFileContents)
{
    run(R"(
        let p = fs.join(dir, "text.txt");
        let q = fs.join(dir, "binary.bin");
        fs.write(p, "hello");
        fs.write(q, "a" + string.char(0) + string.char(255) + "b" + string.char(0));
        return fs.read(p), fs.read(q);
    )",
        2);
    EXPECT_EQ(behl::to_string(S, -2), "hello");
    EXPECT_EQ(behl::to_string(S, -1), "a\0\xff"
                                      "b\0"sv);
}

TEST_P(FsTest, BinaryRoundTripThroughHandles)
{
    run(R"(
        let p = fs.join(dir, "binary.bin");
        let bin = "a" + string.char(0) + string.char(255) + "b" + string.char(0);
        let w = fs.open(p, "w");
        let ok = w:write(bin);
        w:close();
        let f = fs.open(p, "r");
        let content, n = f:read(100);
        f:close();
        return ok, content, n, fs.size(p);
    )",
        4);
    EXPECT_TRUE(behl::to_boolean(S, -4));
    EXPECT_EQ(behl::to_string(S, -3), "a\0\xff"
                                      "b\0"sv);
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 5.0);
}

TEST_P(FsTest, WriteAndAppendConvenienceFunctionsKeepBinaryData)
{
    run(R"(
        let p = fs.join(dir, "binary.bin");
        let bin = "a" + string.char(0) + "b";
        let w = fs.write(p, bin);
        let a = fs.append(p, string.char(0) + "z");
        let f = fs.open(p, "r");
        let content, n = f:read(100);
        f:close();
        return w, a, content, n;
    )",
        4);
    EXPECT_TRUE(behl::to_boolean(S, -4));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_EQ(behl::to_string(S, -2), "a\0b\0z"sv);
    EXPECT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(FsTest, ReadZeroBytesDoesNotAdvance)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "0123456789");
        let f = fs.open(p, "r");
        let c0, n0 = f:read(0);
        let c3, n3 = f:read(3);
        f:close();
        return c0, n0, c3, n3;
    )",
        4);
    EXPECT_EQ(behl::to_string(S, -4), "");
    EXPECT_EQ(behl::to_integer(S, -3), 0);
    EXPECT_EQ(behl::to_string(S, -2), "012");
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(FsTest, ReadAtEofReturnsEmpty)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "0123456789");
        let f = fs.open(p, "r");
        let all, n = f:read(100);
        let rest, m = f:read(5);
        f:close();
        return all, n, rest, m;
    )",
        4);
    EXPECT_EQ(behl::to_string(S, -4), "0123456789");
    EXPECT_EQ(behl::to_integer(S, -3), 10);
    EXPECT_EQ(behl::to_string(S, -2), "");
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(FsTest, ReadNegativeSizeFails)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "0123456789");
        let f = fs.open(p, "r");
        let ok, err = f:read(-1);
        f:close();
        return ok, err;
    )",
        2);
    EXPECT_EQ(behl::type(S, -2), behl::Type::kBoolean);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "invalid size");
}

TEST_P(FsTest, OperationsAfterCloseFail)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let f = fs.open(p, "w");
        let c1 = f:close();
        let c2 = f:close();
        let w, we = f:write("x");
        let r, re = f:read(1);
        let s, se = f:seek("set", 0);
        return c1, c2, w, we, r, re, s, se;
    )",
        8);
    EXPECT_TRUE(behl::to_boolean(S, -8));
    EXPECT_TRUE(behl::to_boolean(S, -7));
    EXPECT_FALSE(behl::to_boolean(S, -6));
    EXPECT_EQ(behl::to_string(S, -5), "file is closed");
    EXPECT_FALSE(behl::to_boolean(S, -4));
    EXPECT_EQ(behl::to_string(S, -3), "file is closed");
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "file is closed");
}

TEST_P(FsTest, WriteOnReadHandleFailsAndLeavesFileUnchanged)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "0123456789");
        let f = fs.open(p, "r");
        let ok = f:write("zz");
        f:close();
        let g = fs.open(p, "r");
        let content = g:read(100);
        g:close();
        return ok, content;
    )",
        2);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "0123456789");
}

TEST_P(FsTest, ReadOnWriteHandleReturnsNoDataAndLeavesFileUnchanged)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let f = fs.open(p, "w");
        f:write("0123456789");
        let c, n = f:read(3);
        f:close();
        let g = fs.open(p, "r");
        let content = g:read(100);
        g:close();
        return c == false || n == 0, content;
    )",
        2);
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "0123456789");
}

TEST_P(FsTest, OpenModes)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        function slurp() {
            let h = fs.open(p, "r");
            let c = h:read(100);
            h:close();
            return c;
        }
        let w = fs.open(p, "w");
        w:write("0123456789");
        w:close();
        let a = fs.open(p, "a");
        a:write("Q");
        a:close();
        let after_a = slurp();
        let rp = fs.open(p, "r+");
        rp:write("X");
        rp:close();
        let after_rp = slurp();
        let wp = fs.open(p, "w+");
        wp:write("hello");
        let wp_pos = wp:seek("set", 0);
        let wp_read = wp:read(100);
        wp:close();
        let ap = fs.open(p, "a+");
        ap:write("!");
        ap:seek("set", 0);
        let ap_read = ap:read(100);
        ap:close();
        return after_a, after_rp, wp_pos, wp_read, ap_read;
    )",
        5);
    EXPECT_EQ(behl::to_string(S, -5), "0123456789Q");
    EXPECT_EQ(behl::to_string(S, -4), "X123456789Q");
    EXPECT_EQ(behl::to_integer(S, -3), 0);
    EXPECT_EQ(behl::to_string(S, -2), "hello");
    EXPECT_EQ(behl::to_string(S, -1), "hello!");
}

TEST_P(FsTest, InvalidModeFails)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        let a, ea = fs.open(p, "rw");
        let b, eb = fs.open(p, "");
        let c, ec = fs.open(p, "rb");
        return a, ea, b, eb, c, ec, fs.exists(p);
    )",
        7);
    for (int idx = -7; idx < -1; idx += 2)
    {
        EXPECT_EQ(behl::type(S, idx), behl::Type::kBoolean);
        EXPECT_FALSE(behl::to_boolean(S, idx));
        EXPECT_NE(behl::to_string(S, idx + 1).find("invalid mode"), std::string_view::npos);
    }
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(FsTest, RenameThenRenameAgainFails)
{
    run(R"(
        let p = fs.join(dir, "old.txt");
        let q = fs.join(dir, "new.txt");
        fs.write(p, "x");
        let first = fs.rename(p, q);
        let again, err = fs.rename(p, q);
        return first, fs.exists(p), fs.exists(q), again, err;
    )",
        5);
    EXPECT_TRUE(behl::to_boolean(S, -5));
    EXPECT_FALSE(behl::to_boolean(S, -4));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1));
}

TEST_P(FsTest, RemoveThenOpenFails)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "x");
        let first = fs.remove(p);
        let second = fs.remove(p);
        let f, err = fs.open(p, "r");
        return first, second, fs.exists(p), f, err;
    )",
        5);
    EXPECT_TRUE(behl::to_boolean(S, -5));
    EXPECT_EQ(behl::type(S, -4), behl::Type::kBoolean);
    EXPECT_FALSE(behl::to_boolean(S, -4));
    EXPECT_FALSE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1));
}

TEST_P(FsTest, DirectoryOperations)
{
    run(R"(
        let sub = fs.join(dir, "sub");
        let nested = fs.join(dir, "a", "b", "c");
        let r = "";
        r = r + (fs.mkdir(sub) ? "T" : "F");
        r = r + (fs.mkdir(sub) ? "T" : "F");
        r = r + (fs.is_dir(sub) ? "T" : "F");
        r = r + (fs.is_file(sub) ? "T" : "F");
        r = r + (fs.exists(sub) ? "T" : "F");
        r = r + (fs.mkdir(nested) ? "T" : "F");
        r = r + (fs.is_dir(nested) ? "T" : "F");
        let removed, err = fs.rmdir(fs.join(dir, "a"));
        r = r + (removed ? "T" : "F");
        r = r + (fs.rmdir(fs.join(dir, "missing")) ? "T" : "F");
        r = r + (fs.rmdir(sub) ? "T" : "F");
        r = r + (fs.exists(sub) ? "T" : "F");
        return r, err;
    )",
        2);
    EXPECT_EQ(behl::to_string(S, -2), "TFTFTTTFFTF");
    EXPECT_TRUE(is_meaningful_error(S, -1));
}

TEST_P(FsTest, FileInformation)
{
    run(R"(
        let p = fs.join(dir, "data.txt");
        fs.write(p, "hello");
        let missing, err = fs.size(fs.join(dir, "missing.txt"));
        return fs.exists(p), fs.is_file(p), fs.is_dir(p), fs.size(p), missing, err;
    )",
        6);
    EXPECT_TRUE(behl::to_boolean(S, -6));
    EXPECT_TRUE(behl::to_boolean(S, -5));
    EXPECT_FALSE(behl::to_boolean(S, -4));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -3), 5.0);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_TRUE(is_meaningful_error(S, -1));
}

TEST_P(FsTest, ListReturnsEntries)
{
    run(R"(
        fs.write(fs.join(dir, "x.txt"), "x");
        fs.mkdir(fs.join(dir, "sub"));
        let l = fs.list(dir);
        return #l, l[0], l[1];
    )",
        3);
    EXPECT_EQ(behl::to_integer(S, -3), 2);
    const std::string_view first = behl::to_string(S, -2);
    const std::string_view second = behl::to_string(S, -1);
    EXPECT_TRUE((first == "x.txt" && second == "sub") || (first == "sub" && second == "x.txt")) << first << " " << second;
}

TEST_P(FsTest, PathHelpers)
{
    run(R"(
        return fs.dirname("a/b/c.txt"), fs.basename("a/b/c.txt"), fs.extension("a/b/c.txt"), fs.stem("a/b/c.txt"),
            fs.extension("a/b/c"), fs.dirname("c.txt"), fs.extension("c.tar.gz"), fs.stem("c.tar.gz"),
            fs.join(), fs.join("a"), fs.join("a", "b");
    )",
        11);
    EXPECT_EQ(behl::to_string(S, -11), "a/b");
    EXPECT_EQ(behl::to_string(S, -10), "c.txt");
    EXPECT_EQ(behl::to_string(S, -9), ".txt");
    EXPECT_EQ(behl::to_string(S, -8), "c");
    EXPECT_EQ(behl::to_string(S, -7), "");
    EXPECT_EQ(behl::to_string(S, -6), "");
    EXPECT_EQ(behl::to_string(S, -5), ".gz");
    EXPECT_EQ(behl::to_string(S, -4), "c.tar");
    EXPECT_EQ(behl::to_string(S, -3), "");
    EXPECT_EQ(behl::to_string(S, -2), "a");
#ifdef _WIN32
    EXPECT_EQ(behl::to_string(S, -1), "a\\b");
#else
    EXPECT_EQ(behl::to_string(S, -1), "a/b");
#endif
}

INSTANTIATE_TEST_SUITE_P(Mode, FsTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
