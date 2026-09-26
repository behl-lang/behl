#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>

namespace behl
{
    class GCTest : public ::testing::TestWithParam<bool>
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            S->jit_enabled = GetParam();
            load_stdlib(S);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_P(GCTest, CollectGarbageFreesUnreachableObjects)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, ReachableObjectsNotCollected)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, UpvaluesPreservedAcrossCollection)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, MultipleClosuresShareUpvalue)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, TableWithCircularReferenceCollected)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, StringInterningSurvivesCollection)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, NestedUpvaluesPreserved)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, TableInClosurePreserved)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, LargeObjectAllocationAndCollection)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, IncrementalGCMakesProgress)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GCDuringTableConstruction)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, ClosureArraySurvivesCollection)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, TemporaryClosuresCollected)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, MutuallyRecursiveClosuresPreserved)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, TableMetatablePreservedDuringGC)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, FunctionPrototypesReusedCorrectly)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GCThresholdCanBeAdjusted)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, EmptyTableStillCollected)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GCDuringRecursion)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, MultipleGCCyclesStable)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GCCountAllReportsCorrectly)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            let count_in_use = gc.count();
            let count_all = gc.countall();
            let count_free = gc.countfree();
            
            return count_all == (count_in_use + count_free);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, FreedObjectsReportedCorrectly)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GCPhaseReturnsValidValue)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc");
            for (let i = 0; i < 100; i++) {
                let temp = {i, i * 2};
            }
            
            let phase = gc.phase();
            
            return phase == "idle" || phase == "mark" || phase == "sweep";
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, ComplexNestedStructurePreserved)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, FunctionArgumentsNotPrematurelyCollected)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, TableArrayResizeDoesntLeak)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, ClosureModifyingUpvalueAcrossGC)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, GlobalsNotCollectedDuringAgressiveGC)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, DeepCallStackWithGC)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, MultipleStringConcatenationsWithGC)
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

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_P(GCTest, ReparentingDuringMarkPhaseKeepsObjectsAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) {
                    t[i] = {id = b * 100 + i}
                }
                buckets[b] = t
            }

            let dst = {}
            let ids = {}
            let churn = {}
            let moved = 0
            let bi = 0
            let ii = 0
            let saw_mark = false

            for (let n = 0; n < 120000; n = n + 1) {
                churn[n % 200] = {pad = n}

                if (gc.phase() == "mark") {
                    saw_mark = true
                    if (bi < 60) {
                        let src = buckets[bi]
                        let o = src[ii]
                        if (o != nil) {
                            dst[moved] = o
                            ids[moved] = bi * 100 + ii
                            src[ii] = nil
                            moved = moved + 1
                        }
                        ii = ii + 1
                        if (ii >= 20) { ii = 0; bi = bi + 1 }
                    }
                }
            }

            gc.collect()
            gc.collect()

            let corrupt = 0
            for (let k = 0; k < moved; k = k + 1) {
                let o = dst[k]
                if (o == nil || o.id != ids[k]) { corrupt = corrupt + 1 }
            }

            return saw_mark, moved, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));

        ASSERT_TRUE(to_boolean(S, -3)) << "workload never reached the mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no objects were reparented during marking";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects were swept while still referenced";
    }

    TEST_P(GCTest, InBoundsArrayStoreDuringMarkPhaseKeepsObjectsAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) {
                    t[i] = {id = b * 100 + i}
                }
                buckets[b] = t
            }

            let dst = {}
            for (let k = 0; k < 1200; k = k + 1) {
                dst[k] = 0
            }
            let ids = {}
            let churn = {}
            let moved = 0
            let bi = 0
            let ii = 0
            let saw_mark = false

            for (let n = 0; n < 120000; n = n + 1) {
                churn[n % 200] = {pad = n}

                if (gc.phase() == "mark") {
                    saw_mark = true
                    if (bi < 60) {
                        let src = buckets[bi]
                        let o = src[ii]
                        if (o != nil) {
                            dst[moved] = o
                            ids[moved] = bi * 100 + ii
                            src[ii] = nil
                            moved = moved + 1
                        }
                        ii = ii + 1
                        if (ii >= 20) { ii = 0; bi = bi + 1 }
                    }
                }
            }

            gc.collect()
            gc.collect()

            let corrupt = 0
            for (let k = 0; k < moved; k = k + 1) {
                let o = dst[k]
                if (o == nil || o.id != ids[k]) { corrupt = corrupt + 1 }
            }

            return saw_mark, moved, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));

        ASSERT_TRUE(to_boolean(S, -3)) << "workload never reached the mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no objects were reparented during marking";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects were swept while still referenced";
    }

    TEST_P(GCTest, RawsetDuringMarkPhaseKeepsObjectsAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")
            const table = import("table")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) { t[i] = {id = b * 100 + i} }
                buckets[b] = t
            }

            let dst = {}
            let ids = {}
            let churn = {}
            let moved = 0
            let bi = 0
            let ii = 0
            let saw_mark = false

            for (let n = 0; n < 120000; n = n + 1) {
                churn[n % 200] = {pad = n}
                if (gc.phase() == "mark") {
                    saw_mark = true
                    if (bi < 60) {
                        let src = buckets[bi]
                        let o = src[ii]
                        if (o != nil) {
                            table.rawset(dst, moved, o)
                            ids[moved] = bi * 100 + ii
                            src[ii] = nil
                            moved = moved + 1
                        }
                        ii = ii + 1
                        if (ii >= 20) { ii = 0; bi = bi + 1 }
                    }
                }
            }

            gc.collect()
            gc.collect()

            let corrupt = 0
            for (let k = 0; k < moved; k = k + 1) {
                let o = dst[k]
                if (o == nil || o.id != ids[k]) { corrupt = corrupt + 1 }
            }
            return saw_mark, moved, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never reached the mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "nothing was rawset during marking";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects swept while referenced through rawset";
    }

    TEST_P(GCTest, ClosedUpvalueDuringMarkPhaseKeepsObjectsAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            function capture(o) { return function() { return o.id } }

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) { t[i] = {id = b * 100 + i} }
                buckets[b] = t
            }

            let fns = {}
            let ids = {}
            let churn = {}
            let moved = 0
            let bi = 0
            let ii = 0
            let saw_mark = false

            for (let n = 0; n < 120000; n = n + 1) {
                churn[n % 200] = {pad = n}
                if (gc.phase() == "mark") {
                    saw_mark = true
                    if (bi < 60) {
                        let src = buckets[bi]
                        let o = src[ii]
                        if (o != nil) {
                            fns[moved] = capture(o)
                            ids[moved] = bi * 100 + ii
                            src[ii] = nil
                            moved = moved + 1
                        }
                        ii = ii + 1
                        if (ii >= 20) { ii = 0; bi = bi + 1 }
                    }
                }
            }

            gc.collect()
            gc.collect()

            let corrupt = 0
            for (let k = 0; k < moved; k = k + 1) {
                let f = fns[k]
                if (f == nil || f() != ids[k]) { corrupt = corrupt + 1 }
            }
            return saw_mark, moved, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never reached the mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no upvalues were closed during marking";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects swept while held by a closed upvalue";
    }

    TEST_P(GCTest, MetatableAssignedDuringMarkPhaseKeepsObjectsAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) { t[i] = {tag = b * 100 + i} }
                buckets[b] = t
            }

            let holders = {}
            let ids = {}
            let churn = {}
            let moved = 0
            let bi = 0
            let ii = 0
            let saw_mark = false

            for (let n = 0; n < 120000; n = n + 1) {
                churn[n % 200] = {pad = n}
                if (gc.phase() == "mark") {
                    saw_mark = true
                    if (bi < 60) {
                        let src = buckets[bi]
                        let mt = src[ii]
                        if (mt != nil) {
                            let h = {}
                            setmetatable(h, mt)
                            holders[moved] = h
                            ids[moved] = bi * 100 + ii
                            src[ii] = nil
                            moved = moved + 1
                        }
                        ii = ii + 1
                        if (ii >= 20) { ii = 0; bi = bi + 1 }
                    }
                }
            }

            gc.collect()
            gc.collect()

            let corrupt = 0
            for (let k = 0; k < moved; k = k + 1) {
                let mt = getmetatable(holders[k])
                if (mt == nil || mt.tag != ids[k]) { corrupt = corrupt + 1 }
            }
            return saw_mark, moved, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never reached the mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no metatables were assigned during marking";
        EXPECT_EQ(to_integer(S, -1), 0) << "metatable swept while still attached";
    }

    TEST_P(GCTest, ObjectMovedIntoLocalDuringMarkPhaseKeepsObjectAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) {
                    t[i] = {id = b * 100 + i}
                }
                buckets[b] = t
            }

            let h0 = nil
            let h1 = nil
            let h2 = nil
            let h3 = nil
            let churn = {}
            let grabbed = false
            let prev = gc.phase()
            let sweeps = 0

            for (let n = 0; n < 200000 && sweeps < 3; n = n + 1) {
                churn[n % 200] = {pad = n}
                let p = gc.phase()
                if (!grabbed && p == "mark" && prev != "mark") {
                    h0 = buckets[0][0]
                    buckets[0][0] = nil
                    h1 = buckets[1][1]
                    buckets[1][1] = nil
                    h2 = buckets[2][2]
                    buckets[2][2] = nil
                    h3 = buckets[3][3]
                    buckets[3][3] = nil
                    grabbed = true
                }
                if (grabbed && p == "sweep" && prev != "sweep") {
                    sweeps = sweeps + 1
                }
                prev = p
            }

            gc.collect()

            let corrupt = 0
            if (h0 == nil || h0.id != 0) { corrupt = corrupt + 1 }
            if (h1 == nil || h1.id != 101) { corrupt = corrupt + 1 }
            if (h2 == nil || h2.id != 202) { corrupt = corrupt + 1 }
            if (h3 == nil || h3.id != 303) { corrupt = corrupt + 1 }
            return grabbed, sweeps, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never entered a fresh mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no sweep ran after the objects were moved";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects held only in locals were swept";
    }

    TEST_P(GCTest, ObjectStoredIntoClosedUpvalueDuringMarkPhaseKeepsObjectAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) {
                    t[i] = {id = b * 100 + i}
                }
                buckets[b] = t
            }

            function make_cell() {
                let v = nil
                return function(x) { v = x }, function() { return v }
            }

            let set0, get0 = make_cell()
            let set1, get1 = make_cell()
            let set2, get2 = make_cell()
            let set3, get3 = make_cell()

            let churn = {}
            let grabbed = false
            let prev = gc.phase()
            let sweeps = 0

            for (let n = 0; n < 200000 && sweeps < 3; n = n + 1) {
                churn[n % 200] = {pad = n}
                let p = gc.phase()
                if (!grabbed && p == "mark" && prev != "mark") {
                    set0(buckets[0][0])
                    buckets[0][0] = nil
                    set1(buckets[1][1])
                    buckets[1][1] = nil
                    set2(buckets[2][2])
                    buckets[2][2] = nil
                    set3(buckets[3][3])
                    buckets[3][3] = nil
                    grabbed = true
                }
                if (grabbed && p == "sweep" && prev != "sweep") {
                    sweeps = sweeps + 1
                }
                prev = p
            }

            gc.collect()

            let corrupt = 0
            if (get0() == nil || get0().id != 0) { corrupt = corrupt + 1 }
            if (get1() == nil || get1().id != 101) { corrupt = corrupt + 1 }
            if (get2() == nil || get2().id != 202) { corrupt = corrupt + 1 }
            if (get3() == nil || get3().id != 303) { corrupt = corrupt + 1 }
            return grabbed, sweeps, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never entered a fresh mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no sweep ran after the objects were moved";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects held only in closed upvalues were swept";
    }

    TEST_P(GCTest, ObjectCapturedByNewClosureDuringMarkPhaseKeepsObjectAlive)
    {
        constexpr std::string_view code = R"(
            const gc = import("gc")

            let buckets = {}
            for (let b = 0; b < 60; b = b + 1) {
                let t = {}
                for (let i = 0; i < 20; i = i + 1) {
                    t[i] = {id = b * 100 + i}
                }
                buckets[b] = t
            }

            function capture(o) {
                return function() { return o }
            }

            let c0 = nil
            let c1 = nil
            let c2 = nil
            let c3 = nil
            let churn = {}
            let grabbed = false
            let prev = gc.phase()
            let sweeps = 0

            for (let n = 0; n < 200000 && sweeps < 3; n = n + 1) {
                churn[n % 200] = {pad = n}
                let p = gc.phase()
                if (!grabbed && p == "mark" && prev != "mark") {
                    c0 = capture(buckets[0][0])
                    buckets[0][0] = nil
                    c1 = capture(buckets[1][1])
                    buckets[1][1] = nil
                    c2 = capture(buckets[2][2])
                    buckets[2][2] = nil
                    c3 = capture(buckets[3][3])
                    buckets[3][3] = nil
                    grabbed = true
                }
                if (grabbed && p == "sweep" && prev != "sweep") {
                    sweeps = sweeps + 1
                }
                prev = p
            }

            gc.collect()

            let corrupt = 0
            if (c0() == nil || c0().id != 0) { corrupt = corrupt + 1 }
            if (c1() == nil || c1().id != 101) { corrupt = corrupt + 1 }
            if (c2() == nil || c2().id != 202) { corrupt = corrupt + 1 }
            if (c3() == nil || c3().id != 303) { corrupt = corrupt + 1 }
            return grabbed, sweeps, corrupt
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        ASSERT_TRUE(to_boolean(S, -3)) << "workload never entered a fresh mark phase";
        ASSERT_GT(to_integer(S, -2), 0) << "no sweep ran after the objects were captured";
        EXPECT_EQ(to_integer(S, -1), 0) << "objects held only by freshly created closures were swept";
    }

    TEST_P(GCTest, LoadedAndFailedChunksDoNotLeak)
    {
        gc_collect(S);
        gc_collect(S);
        const size_t base_objects = S->gc.gc_all_objects.count();
        const size_t base_bytes = S->gc.gc_total_bytes;

        int failures = 0;
        for (int i = 0; i < 300; ++i)
        {
            const std::string good = "let t = {v = " + std::to_string(i) + "}\nfunction f(a) { return a + t.v }\nreturn f(1)";
            ASSERT_TRUE(behl_test::load_ok(S, good));
            ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
            pop(S, 1);

            const std::string bad = "let s" + std::to_string(i) + " = \"x\"\nfunction g(a) { return a + }\n";
            if (load_string(S, bad) < 0)
            {
                ++failures;
                pop(S, 1);
            }
        }

        ASSERT_EQ(failures, 300) << "the malformed chunks did not all fail to load";
        ASSERT_EQ(get_top(S), 0);

        gc_collect(S);
        gc_collect(S);

        EXPECT_LE(S->gc.gc_all_objects.count(), base_objects + 64) << "loaded or failed chunks left objects behind";
        EXPECT_LE(S->gc.gc_total_bytes, base_bytes + 256 * 1024) << "loaded or failed chunks left memory behind";
    }

    INSTANTIATE_TEST_SUITE_P(Mode, GCTest, ::testing::Bool(),
        [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });

} // namespace behl