#include <behl/behl.hpp>
#include <gtest/gtest.h>

namespace behl
{
    class GCStressTest : public ::testing::Test
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            load_stdlib(S, true);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_F(GCStressTest, AllocateManyTables)
    {
        constexpr std::string_view code = R"(
            let count = 0;
            for (let i = 0; i < 1000; i++) {
                let t = {1, 2, 3, 4, 5};
                count = count + 1;
            }
            return count;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(type(S, -1), Type::kInteger);
        EXPECT_EQ(to_integer(S, -1), 1000);
    }

    TEST_F(GCStressTest, AllocateManyStrings)
    {
        constexpr std::string_view code = R"(
            let count = 0;
            for (let i = 0; i < 500; i++) {
                let s = "test string number " + tostring(i);
                count = count + 1;
            }
            return count;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(type(S, -1), Type::kInteger);
        EXPECT_EQ(to_integer(S, -1), 500);
    }

    TEST_F(GCStressTest, NestedTableCreation)
    {
        constexpr std::string_view code = R"(
            function createNested(depth) {
                if (depth == 0) {
                    return {value = 42};
                }
                return {child = createNested(depth - 1)};
            }
            
            return createNested(20);
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(type(S, -1), Type::kTable);
    }

    TEST_F(GCStressTest, TableChurnWithGC)
    {
        constexpr std::string_view code = R"(
            let before = gc.count();
            
            for (let i = 0; i < 100; i++) {
                let temp = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                if (i % 10 == 0) {
                    gc.collect();
                }
            }
            
            gc.collect();
            let after = gc.count();
            
            return after <= before + 5;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, ClosureRetentionAcrossGC)
    {
        constexpr std::string_view code = R"(
            function makeCounter(start) {
                let count = start;
                return function() {
                    count = count + 1;
                    return count;
                };
            }
            
            let counter1 = makeCounter(0);
            let counter2 = makeCounter(100);
            
            for (let i = 0; i < 50; i++) {
                let temp = {a = i, b = i * 2};
            }
            gc.collect();
            
            let v1 = counter1();
            let v2 = counter2();
            
            return v1 == 1 && v2 == 101;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, GlobalTablesNotCollected)
    {
        constexpr std::string_view code = R"(
            global_table = {1, 2, 3, 4, 5};
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2, i * 3};
            }
            
            gc.collect();
            
            return global_table[0] == 1 && global_table[4] == 5;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, LargeTableArray)
    {
        constexpr std::string_view code = R"(
            let big = {};
            for (let i = 0; i < 1000; i++) {
                big[i] = i * i;
            }
            
            gc.collect();
            
            return big[0] == 0 && big[10] == 100 && big[999] == 998001;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, IncrementalGCSteps)
    {
        constexpr std::string_view code = R"(
            let initial_count = gc.count();
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i + 1, i + 2};
            }
            
            let after_alloc = gc.count();
            
            for (let i = 0; i < 50; i++) {
                gc.step();
            }
            
            let after_steps = gc.count();
            
            return after_steps <= after_alloc;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, CircularReferences)
    {
        constexpr std::string_view code = R"(
            function createCircle() {
                let a = {name = "a"};
                let b = {name = "b"};
                a["ref"] = b;
                b["ref"] = a;
                return nil;
            }
            
            let before = gc.count();
            
            for (let i = 0; i < 50; i++) {
                createCircle();
            }
            
            gc.collect();
            let after = gc.count();
            
            return after <= before + 5;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, MixedAllocationPattern)
    {
        constexpr std::string_view code = R"(
            let keepers = {};
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2};
                
                if (i % 10 == 0) {
                    keepers[i / 10] = temp;
                }
                
                let s = "iteration " + tostring(i);
            }
            
            gc.collect();
            
            return keepers[0][0] == 0 && keepers[5][0] == 50;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, GCThresholdAdjustment)
    {
        constexpr std::string_view code = R"(
            let original = gc.threshold();
            gc.setthreshold(10);
            let new_threshold = gc.threshold();
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i * 2, i * 3};
            }
            
            gc.collect();
            gc.setthreshold(original);
            
            return new_threshold == 10;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, DeepRecursionWithAllocation)
    {
        constexpr std::string_view code = R"(
            function recurse(n) {
                if (n == 0) {
                    return {base = true};
                }
                let temp = {level = n};
                return recurse(n - 1);
            }
            
            let result = recurse(50);
            gc.collect();
            
            return result["base"] == true;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, TableMetadataPreservation)
    {
        constexpr std::string_view code = R"(
            let data = {};
            for (let i = 0; i < 100; i++) {
                data["key" + tostring(i)] = i * i;
            }
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i + 1};
            }
            
            gc.collect();
            
            return data["key0"] == 0 && data["key50"] == 2500 && data["key99"] == 9801;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, FreePoolReuse)
    {
        constexpr std::string_view code = R"(
            for (let round = 0; round < 10; round++) {
                for (let i = 0; i < 20; i++) {
                    let temp = {i, i * 2};
                }
                gc.collect();
            }
            
            return true;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCStressTest, GCPhaseCycle)
    {
        constexpr std::string_view code = R"(
            let phase1 = gc.phase();
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i * 2};
            }
            
            gc.step();
            let phase2 = gc.phase();
            
            gc.collect();
            let phase3 = gc.phase();
            
            return phase3 == "idle";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

} // namespace behl
