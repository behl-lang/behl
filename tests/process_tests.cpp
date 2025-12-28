#include "common/format.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string_view>

#ifdef _WIN32
constexpr std::string_view TEST_SHELL = "cmd";
constexpr std::string_view TEST_SHELL_FLAG = "/c";
constexpr std::string_view TEST_SLEEP_CMD = "timeout";
constexpr std::string_view TEST_SLEEP_ARG = "/t";
#else
constexpr std::string_view TEST_SHELL = "sh";
constexpr std::string_view TEST_SHELL_FLAG = "-c";
constexpr std::string_view TEST_SLEEP_CMD = "sleep";
#endif

class ProcessTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S, true);
        behl::load_lib_process(S, true);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(ProcessTest, BasicSpawnAndWait)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "exit 0"}});
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(ProcessTest, CustomExitCode)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "exit 42"}});
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ProcessTest, CaptureStdout)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo test_output"}}, {{stdout = "pipe"}});
        proc:wait();
        let output = proc:read(100);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("test_output") != std::string_view::npos);
}

TEST_F(ProcessTest, ExecCapturesOutput)
{
    auto code = behl::format(R"(
        let result = process.exec("{}", {{"{}", "echo hello"}});
        return result["stdout"];
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("hello") != std::string_view::npos);
}

TEST_F(ProcessTest, ExecReturnsExitCode)
{
    auto code = behl::format(R"(
        let result = process.exec("{}", {{"{}", "exit 7"}});
        return result["exitcode"];
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7);
}

TEST_F(ProcessTest, GetPid)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "exit 0"}});
        let pid = proc:get_pid();
        proc:wait();
        return pid;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_GT(behl::to_integer(S, -1), 0);
}

TEST_F(ProcessTest, IsRunning)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "2"}}, {{stdout = "null"}});
        let running_before = proc:is_running();
        proc:kill();
        proc:wait();
        let running_after = proc:is_running();
        return running_before && !running_after;
    )",
        TEST_SLEEP_CMD, TEST_SLEEP_ARG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"2"}}, {{stdout = "null"}});
        let running_before = proc:is_running();
        proc:kill();
        proc:wait();
        let running_after = proc:is_running();
        return running_before && !running_after;
    )",
        TEST_SLEEP_CMD);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(ProcessTest, ForceKillExitCode)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "10"}}, {{stdout = "null"}});
        proc:kill();
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SLEEP_CMD, TEST_SLEEP_ARG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"10"}}, {{stdout = "null"}});
        proc:kill();
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SLEEP_CMD);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    // Exit code should be 137 (128 + SIGKILL(9))
    EXPECT_EQ(behl::to_integer(S, -1), 137);
}

TEST_F(ProcessTest, SignalTermExitCode)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "10"}}, {{stdout = "null"}});
        proc:signal(process.SIGTERM);
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SLEEP_CMD, TEST_SLEEP_ARG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"10"}}, {{stdout = "null"}});
        proc:signal(process.SIGTERM);
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SLEEP_CMD);
#endif

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 143);
}

TEST_F(ProcessTest, PlatformConstant)
{
    constexpr std::string_view code = R"(
        let platform = process.platform();
        return platform;
    )";

    ASSERT_NO_THROW(load_string(S, code.data()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto platform = to_string(S, -1);
#ifdef _WIN32
    EXPECT_EQ(platform, "windows");
#elif defined(__APPLE__)
    EXPECT_EQ(platform, "darwin");
#else
    EXPECT_EQ(platform, "linux");
#endif
}

TEST_F(ProcessTest, SignalConstants)
{
    constexpr std::string_view code = R"(
        return process.SIGTERM;
    )";

    ASSERT_NO_THROW(load_string(S, code.data()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 15);
}

TEST_F(ProcessTest, StdinPipe)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "exit 0"}}, {{stdin = "pipe"}});
        let written = proc:write("test data\n");
        proc:wait();
        return written > 0;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_F(ProcessTest, NullStdio)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo test"}}, {{stdout = "null", stderr = "null"}});
        let exitcode = proc:wait();
        return exitcode;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    auto exitcode = to_integer(S, -1);
    EXPECT_GE(exitcode, 0);
}

TEST_F(ProcessTest, CloseHandle)
{
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "exit 0"}});
        proc:wait();
        let result = proc:close();
        return result;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(ProcessTest, InvalidCommand)
{
    constexpr std::string_view code = R"(
        let result = process.spawn("nonexistent_command_xyz123", {});
        return typeof(result) == "boolean" && result == false;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code.data()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(ProcessTest, CustomEnvironmentVariable)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo %CUSTOM_VAR%"}}, {{stdout = "pipe", env = {{CUSTOM_VAR = "test_value"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo $CUSTOM_VAR"}}, {{stdout = "pipe", env = {{CUSTOM_VAR = "test_value"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("test_value") != std::string_view::npos);
}

TEST_F(ProcessTest, CustomEnvironmentMultipleVariables)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo %VAR1%-%VAR2%"}}, {{stdout = "pipe", env = {{VAR1 = "hello", VAR2 = "world"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo $VAR1-$VAR2"}}, {{stdout = "pipe", env = {{VAR1 = "hello", VAR2 = "world"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("hello") != std::string_view::npos);
    EXPECT_TRUE(output.find("world") != std::string_view::npos);
}

TEST_F(ProcessTest, InheritedEnvironment)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo %PATH%"}}, {{stdout = "pipe"}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo $PATH"}}, {{stdout = "pipe"}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.size() > 3);
}

TEST_F(ProcessTest, ExecWithCustomEnvironment)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let result = process.exec("{}", {{"{}", "echo %TEST_ENV%"}}, {{env = {{TEST_ENV = "exec_value"}}}});
        return result["stdout"];
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#else
    auto code = behl::format(R"(
        let result = process.exec("{}", {{"{}", "echo $TEST_ENV"}}, {{env = {{TEST_ENV = "exec_value"}}}});
        return result["stdout"];
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#endif

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("exec_value") != std::string_view::npos);
}

TEST_F(ProcessTest, CustomEnvironmentIsolatesFromParent)
{
#ifdef _WIN32
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo %ONLY_VAR%"}}, {{stdout = "pipe", env = {{ONLY_VAR = "isolated"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#else
    auto code = behl::format(R"(
        let proc = process.spawn("{}", {{"{}", "echo $ONLY_VAR"}}, {{stdout = "pipe", env = {{ONLY_VAR = "isolated"}}}});
        proc:wait();
        let output = proc:read(1000);
        return output;
    )",
        TEST_SHELL, TEST_SHELL_FLAG);
#endif

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    auto output = to_string(S, -1);
    EXPECT_TRUE(output.find("isolated") != std::string_view::npos);
}
