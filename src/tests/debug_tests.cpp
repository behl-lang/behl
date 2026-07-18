#include <behl/behl.hpp>
#include <behl/debug.hpp>
#include <gtest/gtest.h>
#include <queue>
#include <vector>

using namespace behl;

struct DebugTestHarness
{
    std::queue<std::string> commands;
    std::vector<int> breakpoint_hits;
    std::vector<DebugEvent> events;
    std::vector<std::string> locations;
    State* state = nullptr;

    static DebugTestHarness* current_instance;

    static void event_callback(State* S, DebugEvent event)
    {
        auto* harness = current_instance;
        if (!harness)
        {
            debug_continue(S);
            return;
        }

        harness->events.push_back(event);

        const char* file = nullptr;
        int line = 0;
        if (debug_get_location(S, &file, &line, nullptr))
        {
            harness->breakpoint_hits.push_back(line);
            if (file)
            {
                harness->locations.push_back(std::string(file) + ":" + std::to_string(line));
            }
        }

        if (!harness->commands.empty())
        {
            std::string cmd = harness->commands.front();
            harness->commands.pop();

            if (cmd == "continue" || cmd == "c")
            {
                debug_continue(S);
            }
            else if (cmd == "step" || cmd == "s")
            {
                debug_step_into(S);
            }
            else if (cmd == "next" || cmd == "n")
            {
                debug_step_over(S);
            }
            else if (cmd == "finish" || cmd == "f")
            {
                debug_step_out(S);
            }
        }
        else
        {
            debug_continue(S);
        }
    }

    void setup(State* S)
    {
        state = S;
        current_instance = this;
        debug_enable(S, true);
        debug_set_event_callback(S, event_callback);
    }

    void teardown()
    {
        if (state)
        {
            debug_enable(state, false);
            current_instance = nullptr;
            state = nullptr; // Clear to prevent double teardown
        }
    }

    ~DebugTestHarness()
    {
        if (current_instance == this)
        {
            current_instance = nullptr;
        }
    }
};

DebugTestHarness* DebugTestHarness::current_instance = nullptr;

class DebugTest : public ::testing::Test
{
protected:
    State* S = nullptr;
    DebugTestHarness harness;

    void SetUp() override
    {
        S = new_state();
        load_stdlib(S);
        harness.setup(S);
    }

    void TearDown() override
    {
        harness.teardown();
        if (S)
        {
            close(S);
            S = nullptr;
        }
    }
};

TEST_F(DebugTest, BasicBreakpoint)
{
    harness.commands.push("continue");

    debug_set_breakpoint(S, nullptr, 2); // Line 2 is "let x = 1;"

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
        let result = x + y;
        let z = 4;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    EXPECT_EQ(harness.breakpoint_hits.size(), 1);
    EXPECT_EQ(harness.breakpoint_hits[0], 2);
}

TEST_F(DebugTest, MultipleBreakpoints)
{
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");

    debug_set_breakpoint(S, nullptr, 3);
    debug_set_breakpoint(S, nullptr, 5);
    debug_set_breakpoint(S, nullptr, 6);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
        let z = 3;
        let a = 4;
        let b = 5;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    ASSERT_EQ(harness.breakpoint_hits.size(), 3);
    EXPECT_EQ(harness.breakpoint_hits[0], 3);
    EXPECT_EQ(harness.breakpoint_hits[1], 5);
    EXPECT_EQ(harness.breakpoint_hits[2], 6);
}

TEST_F(DebugTest, StepInto)
{
    harness.commands.push("s");
    harness.commands.push("s");
    harness.commands.push("s");
    harness.commands.push("c");

    debug_set_breakpoint(S, nullptr, 3);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
        let z = 3;
        let a = 4;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    ASSERT_GE(harness.breakpoint_hits.size(), 3);
    EXPECT_EQ(harness.breakpoint_hits[0], 3);
}

TEST_F(DebugTest, StepOver)
{
    harness.commands.push("n");
    harness.commands.push("n");
    harness.commands.push("c");

    debug_set_breakpoint(S, nullptr, 6);

    constexpr std::string_view code = R"(
        function foo() {
            return 42;
        }
        let x = foo();
        let y = 10;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    ASSERT_GE(harness.breakpoint_hits.size(), 1);
    EXPECT_EQ(harness.breakpoint_hits[0], 6);
}

TEST_F(DebugTest, BreakpointInLoop)
{
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");
    harness.commands.push("c");

    debug_set_breakpoint(S, nullptr, 3);

    constexpr std::string_view code = R"(
        for (let i = 0; i < 3; i++) {
            let x = i;
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    EXPECT_GE(harness.breakpoint_hits.size(), 3);
    for (int hit : harness.breakpoint_hits)
    {
        EXPECT_EQ(hit, 3);
    }
}

TEST_F(DebugTest, RemoveBreakpoint)
{
    harness.commands.push("c");

    debug_set_breakpoint(S, nullptr, 4);
    debug_remove_breakpoint(S, nullptr, 4);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
        let z = 3;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    EXPECT_EQ(harness.breakpoint_hits.size(), 0);
}

TEST_F(DebugTest, PauseExecution)
{
    harness.commands.push("c");

    debug_pause(S);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    ASSERT_GE(harness.breakpoint_hits.size(), 1);
}

TEST_F(DebugTest, ClearAllBreakpoints)
{
    debug_set_breakpoint(S, nullptr, 3);
    debug_set_breakpoint(S, nullptr, 4);
    debug_set_breakpoint(S, nullptr, 5);
    debug_clear_breakpoints(S);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
        let z = 3;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    EXPECT_EQ(harness.breakpoint_hits.size(), 0);
}

TEST(DebugStandaloneTest, GetLocation)
{
    State* S = new_state();
    load_stdlib(S);

    debug_enable(S, true);
    debug_set_event_callback(S, [](State* state, DebugEvent) {
        const char* file = nullptr;
        int line = 0;
        int column = 0;

        bool has_location = debug_get_location(state, &file, &line, &column);
        EXPECT_TRUE(has_location);

        if (has_location)
        {
            EXPECT_NE(file, nullptr);
            EXPECT_GT(line, 0);
        }

        debug_continue(state);
    });

    debug_set_breakpoint(S, nullptr, 2);

    constexpr std::string_view code = R"(
        let x = 1;
        let y = 2;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));

    call(S, 0, 0);

    debug_enable(S, false);
    close(S);
}

TEST(DebugStandaloneTest, IsEnabled)
{
    State* S = new_state();

    EXPECT_FALSE(debug_is_enabled(S));

    debug_enable(S, true);
    EXPECT_TRUE(debug_is_enabled(S));

    debug_enable(S, false);
    EXPECT_FALSE(debug_is_enabled(S));

    close(S);
}
