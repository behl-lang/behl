#include <behl/behl.hpp>
#include <gtest/gtest.h>

using namespace behl;

class RegisterTest : public ::testing::Test
{
protected:
    State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
            S = nullptr;
        }
    }
};

TEST_F(RegisterTest, OuterScopeVariableNotCorruptedByNestedTable)
{
    const char* source = R"(
        function test() {
            let persistent = {cache = {}};
            let i = 0;
            while(i < 2) {
                let nested = {
                    outer = {
                        inner = {value = i}
                    }
                };
                i = i + 1;
            }
            return persistent.cache;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);
    behl::pop(S, 1);
}

TEST_F(RegisterTest, MultipleOuterVariablesPreservedAcrossLoops)
{
    const char* source = R"(
        function test() {
            let a = {x = 1};
            let b = {y = 2};
            let c = {z = 3};
            
            let i = 0;
            while(i < 3) {
                let temp = {data = i};
                i = i + 1;
            }
            
            return a.x + b.y + c.z;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(6, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, NestedLoopsPreserveOuterScopeVariables)
{
    const char* source = R"(
        function test() {
            let outer = {value = 100};
            let sum = 0;
            
            let i = 0;
            while(i < 2) {
                let j = 0;
                while(j < 2) {
                    let nested = {x = i, y = j};
                    sum = sum + outer.value;
                    j = j + 1;
                }
                i = i + 1;
            }
            
            return sum;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(400, behl::to_integer(S, -1)); // 100 * 4 iterations
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ComplexExpressionsWithOuterScope)
{
    const char* source = R"(
        function test() {
            let a = {val = 10};
            let b = {val = 20};
            let c = {val = 30};
            
            let i = 0;
            while(i < 2) {
                let temp1 = {x = 1};
                let temp2 = {y = 2};
                let temp3 = {z = 3};
                let result = a.val + b.val + c.val;
                i = i + 1;
            }
            
            return a.val + b.val + c.val;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(60, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, DeeplyNestedTablesPreserveOuter)
{
    const char* source = R"(
        function test() {
            let persistent = {id = 999};
            
            let i = 0;
            while(i < 2) {
                let level1 = {
                    level2 = {
                        level3 = {
                            level4 = {
                                level5 = {value = i}
                            }
                        }
                    }
                };
                i = i + 1;
            }
            
            return persistent.id;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(999, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, FunctionCallsPreserveOuterVariables)
{
    const char* source = R"(
        function helper(x) {
            let temp = {val = x};
            return temp.val * 2;
        }
        
        function test() {
            let outer = {value = 50};
            
            let i = 0;
            while(i < 3) {
                let result = helper(i);
                i = i + 1;
            }
            
            return outer.value;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(50, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ClosuresAccessingOuterScope)
{
    const char* source = R"(
        function test() {
            let outer1 = {val = 10};
            let outer2 = {val = 20};
            
            let i = 0;
            while(i < 2) {
                function inner() {
                    return outer1.val + outer2.val;
                }
                let result = inner();
                i = i + 1;
            }
            
            return outer1.val + outer2.val;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(30, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, TableFieldAdditionsPreserveBase)
{
    const char* source = R"(
        function test() {
            let table = {base = 100};
            
            let i = 0;
            while(i < 3) {
                table["field_" + tostring(i)] = i;
                i = i + 1;
            }
            
            return table.base;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(100, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ManyLocalsInSingleScope)
{
    const char* source = R"(
        function test() {
            let v1 = {a = 1};
            let v2 = {b = 2};
            let v3 = {c = 3};
            let v4 = {d = 4};
            let v5 = {e = 5};
            let v6 = {f = 6};
            let v7 = {g = 7};
            let v8 = {h = 8};
            let v9 = {i = 9};
            let v10 = {j = 10};
            
            let sum = v1.a + v2.b + v3.c + v4.d + v5.e + 
                      v6.f + v7.g + v8.h + v9.i + v10.j;
            
            return sum;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(55, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ComplexNestedScopeInteractions)
{
    const char* source = R"(
        function test() {
            let a = {val = 1};
            let b = {val = 2};
            
            let i = 0;
            while(i < 2) {
                let c = {val = 3};
                let d = {val = 4};
                
                let j = 0;
                while(j < 2) {
                    let e = {val = 5};
                    let f = {val = 6};
                    
                    let k = 0;
                    while(k < 2) {
                        let g = {val = 7};
                        let sum = a.val + b.val + c.val + d.val + 
                                  e.val + f.val + g.val;
                        k = k + 1;
                    }
                    j = j + 1;
                }
                i = i + 1;
            }
            
            return a.val + b.val;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(3, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, TableArrayPreservesOuterScope)
{
    const char* source = R"(
        function test() {
            let outer = {id = 123};
            
            let i = 0;
            while(i < 2) {
                let arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                let sum = arr[0] + arr[9];
                i = i + 1;
            }
            
            return outer.id;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(123, behl::to_integer(S, -1));
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ConditionalBranchesPreserveRegisters)
{
    const char* source = R"(
        function test() {
            let outer = {value = 42};
            let sum = 0;
            
            let i = 0;
            while(i < 4) {
                if (i % 2 == 0) {
                    let temp = {x = i};
                    sum = sum + outer.value;
                } else {
                    let temp = {y = i};
                    sum = sum + outer.value;
                }
                i = i + 1;
            }
            
            return sum;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(168, behl::to_integer(S, -1)); // 42 * 4
    behl::pop(S, 1);
}

TEST_F(RegisterTest, OriginalBugReportCase)
{
    const char* source = R"(
        function test() {
            let persistent = {
                config = {name = "test"},
                cache = {},
                stats = {total = 0}
            };
            
            let i = 0;
            while(i < 100) {
                let nested = {
                    outer = {
                        middle = {
                            inner = {data = "deep", value = i}
                        }
                    }
                };
                
                if (i % 25 == 0) {
                    persistent.cache["key_" + tostring(i)] = {value = i};
                    persistent.stats.total = i;
                }
                
                i = i + 1;
            }
            
            return persistent.stats.total;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(75, behl::to_integer(S, -1)); // Last i % 25 == 0 is at i=75
    behl::pop(S, 1);
}

TEST_F(RegisterTest, ExtremeRegisterPressure)
{
    const char* source = R"(
        function test() {
            let o1 = {a = 1};
            let o2 = {b = 2};
            let o3 = {c = 3};
            let o4 = {d = 4};
            let o5 = {e = 5};
            
            let i = 0;
            while(i < 2) {
                let l1 = {x = 1};
                let l2 = {x = 2};
                let l3 = {x = 3};
                
                let j = 0;
                while(j < 2) {
                    let m1 = {y = 1};
                    let m2 = {y = 2};
                    
                    let k = 0;
                    while(k < 2) {
                        let n1 = {z = 1};
                        let n2 = {z = 2};
                        let sum = o1.a + o2.b + o3.c + o4.d + o5.e;
                        k = k + 1;
                    }
                    j = j + 1;
                }
                i = i + 1;
            }
            
            return o1.a + o2.b + o3.c + o4.d + o5.e;
        }
        return test();
    )";

    ASSERT_NO_THROW(behl::load_string(S, source));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(15, behl::to_integer(S, -1));
    behl::pop(S, 1);
}
