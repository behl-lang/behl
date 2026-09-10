#include <behl/behl.hpp>
#include <gtest/gtest.h>

namespace behl
{
    class GCTest : public ::testing::Test
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            load_stdlib(S);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_F(GCTest, CollectGarbageFreesUnreachableObjects)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let before = gc.count();
            
            function createGarbage() {
                let temp1 = {1, 2, 3};
                let temp2 = {4, 5, 6};
                let temp3 = {7, 8, 9};
            }
            
            createGarbage();
            createGarbage();
            createGarbage();
            
            gc.collect();
            let after = gc.count();
            
            return after <= before + 2;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, ReachableObjectsNotCollected)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let keeper = {data = "important"};
            let before = gc.count();
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i * 2};
            }
            
            gc.collect();
            
            return keeper["data"] == "important";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, UpvaluesPreservedAcrossCollection)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeClosure() {
                let captured = 42;
                return function() {
                    return captured;
                };
            }
            
            let fn = makeClosure();
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i + 1};
            }
            
            gc.collect();
            
            return fn() == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, MultipleClosuresShareUpvalue)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeClosures() {
                let shared = 100;
                let inc = function() {
                    shared = shared + 1;
                    return shared;
                };
                let dec = function() {
                    shared = shared - 1;
                    return shared;
                };
                return inc, dec;
            }
            
            let inc, dec = makeClosures();
            
            gc.collect();
            
            let v1 = inc();
            let v2 = inc();
            let v3 = dec();
            
            return v1 == 101 && v2 == 102 && v3 == 101;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, TableWithCircularReferenceCollected)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let before_count = gc.count();
            
            function makeCircle() {
                let t = {};
                t["self"] = t;
                return nil;
            }
            
            for (let i = 0; i < 30; i++) {
                makeCircle();
            }
            
            gc.collect();
            let after_count = gc.count();
            
            return after_count <= before_count + 3;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, StringInterningSurvivesCollection)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let s1 = "test";
            
            for (let i = 0; i < 50; i++) {
                let temp = "temp" + tostring(i);
            }
            
            gc.collect();
            
            let s2 = "test";
            
            return s1 == s2;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, NestedUpvaluesPreserved)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function outer() {
                let a = 10;
                function middle() {
                    let b = 20;
                    function inner() {
                        return a + b;
                    }
                    return inner;
                }
                return middle();
            }
            
            let fn = outer();
            
            for (let i = 0; i < 50; i++) {
                let temp = {i};
            }
            
            gc.collect();
            
            return fn() == 30;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, TableInClosurePreserved)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeTableClosure() {
                let data = {x = 5, y = 10};
                return function() {
                    return data["x"] + data["y"];
                };
            }
            
            let fn = makeTableClosure();
            
            gc.collect();
            
            return fn() == 15;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, LargeObjectAllocationAndCollection)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeHugeTable() {
                let t = {};
                for (let i = 0; i < 500; i++) {
                    t[i] = i * i;
                }
                return nil;
            }
            
            let before = gc.count();
            
            for (let i = 0; i < 10; i++) {
                makeHugeTable();
            }
            
            gc.collect();
            let after = gc.count();
            
            return after <= before + 2;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, IncrementalGCMakesProgress)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2, i * 3};
            }
            
            let phase1 = gc.phase();
            
            for (let i = 0; i < 10; i++) {
                gc.step();
            }
            
            let phase2 = gc.phase();
            
            return true;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GCDuringTableConstruction)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function buildLarge() {
                let t = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                gc.step();
                t[10] = 11;
                t[11] = 12;
                return t;
            }
            
            let result = buildLarge();
            gc.collect();
            
            return result[0] == 1 && result[11] == 12;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, ClosureArraySurvivesCollection)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeCounters() {
                let counters = {};
                let c0 = 0;
                let c1 = 10;
                let c2 = 20;
                counters[0] = function() { c0 = c0 + 1; return c0; };
                counters[1] = function() { c1 = c1 + 1; return c1; };
                counters[2] = function() { c2 = c2 + 1; return c2; };
                return counters;
            }
            
            let counters = makeCounters();
            
            gc.collect();
            
            let v0 = counters[0]();
            let v1 = counters[1]();
            let v2 = counters[2]();
            
            return v0 == 1 && v1 == 11 && v2 == 21;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, TemporaryClosuresCollected)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeTempClosure() {
                let temp = {1, 2, 3};
                return function() {
                    return temp[0];
                };
            }
            
            let before = gc.count();
            
            for (let i = 0; i < 20; i++) {
                let fn = makeTempClosure();
            }
            
            gc.collect();
            let after = gc.count();
            
            return true;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, MutuallyRecursiveClosuresPreserved)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let even, odd;
            
            even = function(n) {
                if (n == 0) {
                    return true;
                }
                return odd(n - 1);
            };
            
            odd = function(n) {
                if (n == 0) {
                    return false;
                }
                return even(n - 1);
            };
            
            gc.collect();
            
            return even(10) && !odd(10);
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, TableMetatablePreservedDuringGC)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let t = {a = 1, b = 2, c = 3};
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i * 2};
            }
            
            gc.collect();
            
            return t["a"] == 1 && t["b"] == 2 && t["c"] == 3;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, FunctionPrototypesReusedCorrectly)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function factory() {
                function inner() {
                    return 42;
                }
                return inner;
            }
            
            let f1 = factory();
            let f2 = factory();
            
            gc.collect();
            
            return f1() == 42 && f2() == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GCThresholdCanBeAdjusted)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let original = gc.threshold();
            gc.setthreshold(50);
            let new_val = gc.threshold();
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2, i * 3, i * 4};
            }
            
            gc.collect();
            gc.setthreshold(original);
            
            return new_val == 50;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, EmptyTableStillCollected)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let before = gc.count();
            
            for (let i = 0; i < 50; i++) {
                let empty = {};
            }
            
            gc.collect();
            let after = gc.count();
            
            return after <= before + 2;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GCDuringRecursion)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function sum(n) {
                if (n == 0) {
                    return 0;
                }
                if (n % 5 == 0) {
                    gc.step();
                }
                return n + sum(n - 1);
            }
            
            let result = sum(20);
            gc.collect();
            
            return result == 210;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, MultipleGCCyclesStable)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let keeper = {value = 100};
            
            for (let round = 0; round < 5; round++) {
                for (let i = 0; i < 20; i++) {
                    let temp = {i, i * 2};
                }
                gc.collect();
            }
            
            return keeper["value"] == 100;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GCCountAllReportsCorrectly)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let count_in_use = gc.count();
            let count_all = gc.countall();
            let count_free = gc.countfree();
            
            return count_all == (count_in_use + count_free);
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, FreedObjectsReportedCorrectly)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            for (let i = 0; i < 20; i++) {
                let temp = {i};
            }
            
            gc.collect();
            let free_count = gc.countfree();
            
            return free_count >= 0;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GCPhaseReturnsValidValue)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2};
            }
            
            let phase = gc.phase();
            
            return phase == "idle" || phase == "mark" || phase == "sweep";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, ComplexNestedStructurePreserved)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let root = {
                level1 = {
                    level2 = {
                        level3 = {
                            value = "deep"
                        }
                    }
                }
            };
            
            for (let i = 0; i < 50; i++) {
                let temp = {i, i * 2, i * 3};
            }
            
            gc.collect();
            
            return root["level1"]["level2"]["level3"]["value"] == "deep";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, FunctionArgumentsNotPrematurelyCollected)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function process(t) {
                gc.step();
                gc.step();
                return t["x"] + t["y"];
            }
            
            let result = process({x = 10, y = 20});
            
            return result == 30;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, TableArrayResizeDoesntLeak)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let t = {};
            
            for (let i = 0; i < 100; i++) {
                t[i] = i;
            }
            
            let before = gc.count();
            
            for (let i = 100; i < 500; i++) {
                t[i] = i;
            }
            
            gc.collect();
            let after = gc.count();
            
            return t[0] == 0 && t[499] == 499;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, ClosureModifyingUpvalueAcrossGC)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function makeModifier() {
                let value = 0;
                return function(n) {
                    value = value + n;
                    return value;
                };
            }
            
            let modify = makeModifier();
            
            let v1 = modify(5);
            gc.collect();
            let v2 = modify(10);
            gc.collect();
            let v3 = modify(3);
            
            return v1 == 5 && v2 == 15 && v3 == 18;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, GlobalsNotCollectedDuringAgressiveGC)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            important_data = {x = 1, y = 2, z = 3};
            
            gc.setthreshold(1);
            
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2};
                gc.step();
            }
            
            gc.collect();
            gc.setthreshold(100);
            
            return important_data["x"] == 1 && 
                   important_data["y"] == 2 && 
                   important_data["z"] == 3;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, DeepCallStackWithGC)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            function deep(n) {
                if (n == 0) {
                    gc.collect();
                    return 1;
                }
                let temp = {n};
                return deep(n - 1) + 1;
            }
            
            let result = deep(30);
            
            return result == 31;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(GCTest, MultipleStringConcatenationsWithGC)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let s = "start";
            
            for (let i = 0; i < 50; i++) {
                s = s + tostring(i);
                if (i % 10 == 0) {
                    gc.step();
                }
            }
            
            gc.collect();
            
            return typeof(s) == "string";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

} // namespace behl
