#include "common/string.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
using namespace behl;

class MetatableTest : public ::testing::TestWithParam<bool>
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

TEST_P(MetatableTest, GetMetatableReturnsNilForNoMetatable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        return getmetatable(t) == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, SetAndGetMetatable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let mt = {__name = "MyTable"}
        setmetatable(t, mt)
        let result = getmetatable(t)
        return result.__name == "MyTable"
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, SetMetatableReturnsTable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let mt = {}
        let result = setmetatable(t, mt)
        return result == t
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, SetMetatableToNilRemovesMetatable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let mt = {__name = "test"}
        setmetatable(t, mt)
        setmetatable(t, nil)
        return getmetatable(t) == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, IndexMetamethodWithFunction)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let mt = {
            __index = function(table, key) {
                if (key == "b") {
                    return 42
                }
                return nil
            }
        }
        setmetatable(t, mt)
        return t.a == 1 && t.b == 42 && t.c == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, IndexMetamethodWithTable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let fallback = {b = 2, c = 3}
        let mt = {__index = fallback}
        setmetatable(t, mt)
        return t.a == 1 && t.b == 2 && t.c == 3
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, IndexMetamethodChaining)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        let parent = {b = 2}
        let grandparent = {c = 3}
        
        setmetatable(parent, {__index = grandparent})
        setmetatable(t, {__index = parent})
        
        return t.a == 1 && t.b == 2 && t.c == 3
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, IndexMetamethodNotCalledForExistingKey)
{
    constexpr std::string_view code = R"(
        let called = false
        let t = {a = 1}
        let mt = {
            __index = function(table, key) {
                called = true
                return 99
            }
        }
        setmetatable(t, mt)
        let val = t.a
        return val == 1 && called == false
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, NewIndexMetamethodWithFunction)
{
    constexpr std::string_view code = R"(
        let storage = {}
        let t = {}
        let mt = {
            __newindex = function(table, key, value) {
                storage[key] = value
            }
        }
        setmetatable(t, mt)
        t.a = 42
        return storage.a == 42 && t.a == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, NewIndexMetamethodWithTable)
{
    constexpr std::string_view code = R"(
        let proxy = {}
        let t = {}
        let mt = {__newindex = proxy}
        setmetatable(t, mt)
        t.a = 123
        return proxy.a == 123 && t.a == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, NewIndexMetamethodNotCalledForExistingKey)
{
    constexpr std::string_view code = R"(
        let called = false
        let t = {a = 1}
        let mt = {
            __newindex = function(table, key, value) {
                called = true
            }
        }
        setmetatable(t, mt)
        t.a = 2
        return t.a == 2 && called == false
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, AddMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 10}
        let t2 = {value = 5}
        let mt = {
            __add = function(a, b) {
                return {value = a.value + b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 + t2
        return result.value == 15
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, SubMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 10}
        let t2 = {value = 3}
        let mt = {
            __sub = function(a, b) {
                return {value = a.value - b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 - t2
        return result.value == 7
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, MulMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 6}
        let t2 = {value = 7}
        let mt = {
            __mul = function(a, b) {
                return {value = a.value * b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 * t2
        return result.value == 42
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DivMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 20}
        let t2 = {value = 4}
        let mt = {
            __div = function(a, b) {
                return {value = a.value / b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 / t2
        return result.value == 5
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ModMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 17}
        let t2 = {value = 5}
        let mt = {
            __mod = function(a, b) {
                return {value = a.value % b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 % t2
        return result.value == 2
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, PowMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 2}
        let t2 = {value = 8}
        let mt = {
            __pow = function(a, b) {
                return {value = a.value ** b.value}
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let result = t1 ** t2
        return result.value == 256
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, UnmMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {value = 10}
        let mt = {
            __unm = function(a) {
                return {value = -a.value}
            }
        }
        setmetatable(t, mt)
        let result = -t
        return result.value == -10
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ArithmeticWithMixedTypes)
{
    constexpr std::string_view code = R"(
        let t = {value = 10}
        let mt = {
            __add = function(a, b) {
                if (typeof(a) == "table") {
                    return {value = a.value + b}
                } else {
                    return {value = a + b.value}
                }
            }
        }
        setmetatable(t, mt)
        let result1 = t + 5
        let result2 = 5 + t
        return result1.value == 15 && result2.value == 15
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, EqMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 10}
        let t2 = {value = 10}
        let t3 = {value = 5}
        let mt = {
            __eq = function(a, b) {
                return a.value == b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        setmetatable(t3, mt)
        return (t1 == t2) && !(t1 == t3)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LtMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        return (t1 < t2) && !(t2 < t1)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LeMetamethod)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let t3 = {value = 5}
        let mt = {
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        setmetatable(t3, mt)
        return (t1 <= t2) && (t1 <= t3) && !(t2 <= t1)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, CallMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {value = 10}
        let mt = {
            __call = function(self, x) {
                return self.value + x
            }
        }
        setmetatable(t, mt)
        let result = t(5)
        return result == 15
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, CallMetamethodWithMultipleArgs)
{
    constexpr std::string_view code = R"(
        let t = {}
        let mt = {
            __call = function(self, a, b, c) {
                return a + b + c
            }
        }
        setmetatable(t, mt)
        let result = t(1, 2, 3)
        return result == 6
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ToStringMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {name = "MyObject"}
        let mt = {
            __tostring = function(self) {
                return "Table: " + self.name
            }
        }
        setmetatable(t, mt)
        let str = tostring(t)
        return str
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "Table: MyObject");
}

TEST_P(MetatableTest, LenMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3}  // Array part with 3 elements
        let mt = {
            __len = function(self) {
                return 999  // Custom length
            }
        }
        setmetatable(t, mt)
        return #t == 999
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, RawGetBypassesMetatable)
{
    constexpr std::string_view code = R"(
        const table = import("table");
        let t = {a = 1}
        let mt = {
            __index = function(table, key) {
                return 999
            }
        }
        setmetatable(t, mt)
        return t.b == 999 && table.rawget(t, "b") == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, RawSetBypassesMetatable)
{
    constexpr std::string_view code = R"(
        const table = import("table");
        let called = false
        let t = {}
        let mt = {
            __newindex = function(table, key, value) {
                called = true
            }
        }
        setmetatable(t, mt)
        table.rawset(t, "a", 42)
        return t.a == 42 && called == false
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, RawLenBypassesMetatable)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3}
        let mt = {
            __len = function(self) {
                return 999
            }
        }
        setmetatable(t, mt)
        return #t == 999 && rawlen(t) == 3
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, SimpleClassPattern)
{
    constexpr std::string_view code = R"(
        let Animal = {
            speak = function(self) {
                return "Some sound"
            }
        }
        Animal.__index = Animal
        
        function newAnimal(name) {
            let obj = {name = name}
            setmetatable(obj, Animal)
            return obj
        }
        
        let cat = newAnimal("Fluffy")
        return cat.name == "Fluffy" && cat.speak(cat) == "Some sound"
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, InheritancePattern)
{
    constexpr std::string_view code = R"(
        let Animal = {
            speak = function(self) {
                return "Some sound"
            }
        }
        Animal.__index = Animal
        
        let Dog = {
            speak = function(self) {
                return "Woof!"
            }
        }
        Dog.__index = Dog
        setmetatable(Dog, Animal)
        
        function newDog(name) {
            let obj = {name = name}
            setmetatable(obj, Dog)
            return obj
        }
        
        let dog = newDog("Rex")
        return dog.name == "Rex" && dog.speak(dog) == "Woof!"
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, MetatableOnMetatable)
{
    constexpr std::string_view code = R"(
        let t = {}
        let mt = {}
        let mtmt = {
            __index = {x = 42}
        }
        setmetatable(mt, mtmt)
        setmetatable(t, mt)
        
        return mt.x == 42
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, MetamethodReturnsMultipleValues)
{
    constexpr std::string_view code = R"(
        let t = {}
        let mt = {
            __index = function(table, key) {
                return 1, 2, 3
            }
        }
        setmetatable(t, mt)
        let a = t.x
        return a == 1
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, RecursiveIndexLookup)
{
    constexpr std::string_view code = R"(
        let t = {}
        let depth = 0
        let mt = {
            __index = function(table, key) {
                depth = depth + 1
                if (depth < 5) {
                    return table[key]  // This would recurse
                }
                return depth
            }
        }
        setmetatable(t, mt)
        let result = t.x
        return result == 5
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ArithmeticMetamethodOnlyOneOperand)
{
    constexpr std::string_view code = R"(
        let t = {value = 10}
        let mt = {
            __add = function(a, b) {
                if (typeof(a) == "table") {
                    return a.value + b
                } else {
                    return a + b.value
                }
            }
        }
        setmetatable(t, mt)
        let result = t + 5
        return result == 15
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ComparisonRequiresBothMetamethods)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 10}
        let t2 = {value = 5}
        let mt1 = {
            __eq = function(a, b) {
                return true
            }
        }
        setmetatable(t1, mt1)
        
        return !(t1 == t2)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DeepNestedAddMetamethod32Levels)
{
    constexpr std::string_view code = R"(
        let call_depth = 0;
        let max_depth = 32;
        
        function nested_call(depth) {
            if (depth >= max_depth) {
                return depth;
            }
            let temp = {x = depth, y = depth * 2, z = depth * 3};
            return nested_call(depth + 1);
        }
        
        let mt = {
            __add = function(a, b) {
                let result_depth = nested_call(0);
                return {value = a.value + b.value + result_depth};
            }
        };
        
        let obj1 = {value = 10};
        let obj2 = {value = 20};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 + obj2;
        return result.value == 62;  // 10 + 20 + 32
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DeepNestedMultipleArithmeticOps)
{
    constexpr std::string_view code = R"(
        let call_count = 0;
        
        function recursive_helper(depth, accumulator) {
            if (depth <= 0) {
                return accumulator;
            }
            let temp1 = {a = depth};
            let temp2 = {b = depth * 2};
            let temp3 = {c = depth * 3};
            return recursive_helper(depth - 1, accumulator + 1);
        }
        
        let mt = {
            __add = function(a, b) {
                call_count = call_count + 1;
                let extra = recursive_helper(35, 0);
                let result = {value = a.value + b.value + extra};
                setmetatable(result, mt);
                return result;
            },
            __mul = function(a, b) {
                call_count = call_count + 1;
                let extra = recursive_helper(35, 0);
                let result = {value = a.value * b.value + extra};
                setmetatable(result, mt);
                return result;
            },
            __sub = function(a, b) {
                call_count = call_count + 1;
                let extra = recursive_helper(35, 0);
                let result = {value = a.value - b.value + extra};
                setmetatable(result, mt);
                return result;
            }
        };
        
        let obj1 = {value = 5};
        let obj2 = {value = 3};
        let obj3 = {value = 2};
        
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        setmetatable(obj3, mt);
        
        let sum = obj1 + obj2;      // 5 + 3 + 35 = 43
        let product = sum * obj3;   // 43 * 2 + 35 = 121
        let diff = product - obj1;  // 121 - 5 + 35 = 151
        
        return diff.value == 151 && call_count == 3;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DeepNestedWithTableCreation)
{
    constexpr std::string_view code = R"(
        function create_deep_structure(depth) {
            if (depth <= 0) {
                return {leaf = true, depth = 0};
            }
            let child = create_deep_structure(depth - 1);
            return {
                depth = depth,
                child = child,
                data1 = {x = depth},
                data2 = {y = depth * 2},
                data3 = {z = depth * 3}
            };
        }
        
        let mt = {
            __add = function(a, b) {
                let structure = create_deep_structure(40);
                
                let current = structure;
                let count = 0;
                while (current.child != nil && count < 50) {
                    current = current.child;
                    count = count + 1;
                }
                
                return {value = a.value + b.value + count};
            }
        };
        
        let obj1 = {value = 100};
        let obj2 = {value = 200};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 + obj2;
        return result.value == 340;  // 100 + 200 + 40
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DeepNestedChainedMetamethods)
{
    constexpr std::string_view code = R"(
        let call_chain = {};
        
        function build_chain(depth) {
            if (depth <= 0) {
                return 1;
            }
            let temp = {
                id = depth,
                nested = {a = depth, b = depth * 2}
            };
            call_chain[depth] = temp;
            return build_chain(depth - 1) + 1;
        }
        
        let mt = {
            __add = function(a, b) {
                let chain_length = build_chain(50);
                let result = {value = a.value + b.value, chain = chain_length};
                setmetatable(result, mt);
                return result;
            },
            __mul = function(a, b) {
                let chain_length = build_chain(50);
                let result = {value = a.value * b.value, chain = chain_length};
                setmetatable(result, mt);
                return result;
            }
        };
        
        let obj1 = {value = 7};
        let obj2 = {value = 3};
        let obj3 = {value = 2};
        
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        setmetatable(obj3, mt);
        
        let sum = obj1 + obj2;      // 7 + 3 = 10, chain = 51
        let result = sum * obj3;    // 10 * 2 = 20, chain = 51
        
        return result.value == 20 && result.chain == 51;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ExtremeMixedOperationsDepth64)
{
    constexpr std::string_view code = R"(
        let global_counter = 0;
        
        function fibonacci_like(n) {
            global_counter = global_counter + 1;
            if (n <= 1) {
                return n;
            }
            let temp = {step = n};
            return fibonacci_like(n - 1) + fibonacci_like(n - 2);
        }
        
        function factorial_like(n) {
            global_counter = global_counter + 1;
            if (n <= 1) {
                return 1;
            }
            let temp = {step = n, data = {x = n}};
            return n * factorial_like(n - 1);
        }
        
        let mt = {
            __add = function(a, b) {
                let fib = fibonacci_like(10);
                let fact = factorial_like(5);
                return {value = a.value + b.value + fib + fact};
            },
            __sub = function(a, b) {
                let fib = fibonacci_like(10);
                return {value = a.value - b.value + fib};
            },
            __mul = function(a, b) {
                let fact = factorial_like(5);
                return {value = a.value * b.value + fact};
            }
        };
        
        let obj1 = {value = 100};
        let obj2 = {value = 50};
        let obj3 = {value = 2};
        
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        setmetatable(obj3, mt);
        
        global_counter = 0;
        
        let result1 = obj1 + obj2;      // 100 + 50 + fib(10) + fact(5)
        let result2 = result1 - obj3;   // result1 - 2 + fib(10)
        let result3 = result2 * obj1;   // result2 * 100 + fact(5)
        
        return global_counter > 100;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, DeepNestedWithUpvalues)
{
    constexpr std::string_view code = R"(
        function create_nested_closures(depth) {
            if (depth <= 0) {
                return function() {
                    return 1;
                };
            }
            
            let captured = depth;
            let inner = create_nested_closures(depth - 1);
            
            return function() {
                let temp = {id = captured, data = {x = captured}};
                return inner() + 1;
            };
        }
        
        let mt = {
            __add = function(a, b) {
                let closure_chain = create_nested_closures(50);
                let closure_result = closure_chain();
                
                return {value = a.value + b.value + closure_result};
            }
        };
        
        let obj1 = {value = 5};
        let obj2 = {value = 10};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 + obj2;
        return result.value == 66;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, StressTestMassiveStackGrowth)
{
    constexpr std::string_view code = R"(
        function allocate_heavy(depth, size) {
            if (depth <= 0) {
                return size;
            }
            
            let arr = {};
            let i = 0;
            while (i < size) {
                arr[i] = {
                    id = i,
                    depth = depth,
                    data = {x = i, y = i * 2, z = i * 3},
                    extra = {a = 1, b = 2, c = 3}
                };
                i = i + 1;
            }
            
            return allocate_heavy(depth - 1, size) + 1;
        }
        
        let mt = {
            __add = function(a, b) {
                let count = allocate_heavy(25, 20);
                return {value = a.value + b.value + count};
            }
        };
        
        let obj1 = {value = 1000};
        let obj2 = {value = 2000};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 + obj2;
        return result.value == 3045;  // 1000 + 2000 + 45 (size 20 + depth 25)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseAndMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __band = function(a, b) {
                return {value = a.value & b.value};
            }
        };
        
        let obj1 = {value = 0xF0};
        let obj2 = {value = 0x0F};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 & obj2;
        return result.value == 0;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseOrMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bor = function(a, b) {
                return {value = a.value | b.value};
            }
        };
        
        let obj1 = {value = 0xF0};
        let obj2 = {value = 0x0F};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 | obj2;
        return result.value == 0xFF;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseXorMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bxor = function(a, b) {
                return {value = a.value ^ b.value};
            }
        };
        
        let obj1 = {value = 0xFF};
        let obj2 = {value = 0xAA};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 ^ obj2;
        return result.value == 0x55;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseLeftShiftMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __shl = function(a, b) {
                return {value = a.value << b.value};
            }
        };
        
        let obj1 = {value = 4};
        let obj2 = {value = 2};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 << obj2;
        return result.value == 16;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseRightShiftMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __shr = function(a, b) {
                return {value = a.value >> b.value};
            }
        };
        
        let obj1 = {value = 16};
        let obj2 = {value = 2};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 >> obj2;
        return result.value == 4;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseNotMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bnot = function(a) {
                return {value = ~a.value};
            }
        };
        
        let obj = {value = 0};
        setmetatable(obj, mt);
        
        let result = ~obj;
        return result.value == -1;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseMixedOperations)
{
    constexpr std::string_view code = R"(
        let mt = {
            __band = function(a, b) {
                return {value = a.value & b.value};
            },
            __bor = function(a, b) {
                return {value = a.value | b.value};
            },
            __bxor = function(a, b) {
                return {value = a.value ^ b.value};
            }
        };
        
        let obj1 = {value = 12};  // 0b1100
        let obj2 = {value = 10};  // 0b1010
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let and_result = obj1 & obj2;  // 8 (0b1000)
        let xor_result = obj1 ^ obj2;  // 6 (0b0110)
        setmetatable(and_result, mt);
        setmetatable(xor_result, mt);
        
        let final = and_result | xor_result;  // 14 (0b1110)
        return final.value == 14;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseDeepNested)
{
    constexpr std::string_view code = R"(
        function create_nested_closures(n) {
            if (n == 0) {
                return function() { return 1; };
            } else {
                let inner = create_nested_closures(n - 1);
                return function() {
                    return inner() + 1;
                };
            }
        }
        
        let mt = {
            __band = function(a, b) {
                let closure_chain = create_nested_closures(100);
                let closure_result = closure_chain();
                return {value = (a.value & b.value) + closure_result};
            }
        };
        
        let obj1 = {value = 0xFF};
        let obj2 = {value = 0x0F};
        setmetatable(obj1, mt);
        setmetatable(obj2, mt);
        
        let result = obj1 & obj2;
        return result.value == 116;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, BitwiseWithUpvalues)
{
    constexpr std::string_view code = R"(
        function make_bitwise_obj(base_val, modifier) {
            let captured_mod = modifier;
            
            return {
                value = base_val,
                __band = function(a, b) {
                    let result = (a.value & b.value) + captured_mod;
                    return {value = result};
                }
            };
        }
        
        let obj1 = make_bitwise_obj(0xFF, 100);
        let obj2 = make_bitwise_obj(0x0F, 50);
        
        setmetatable(obj1, obj1);  // Use self as metatable
        
        let result = obj1 & obj2;
        return result.value == 115;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, MetatableNewCreatesNewMetatable)
{
    bool created = metatable_new(S, "TestMetatable");
    EXPECT_TRUE(created);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_EQ(type(S, -1), Type::kTable);
    pop(S, 1);
}

TEST_P(MetatableTest, MetatableNewReturnsFalseForExisting)
{
    bool created1 = metatable_new(S, "TestMetatable");
    EXPECT_TRUE(created1);
    pop(S, 1);

    bool created2 = metatable_new(S, "TestMetatable");
    EXPECT_FALSE(created2);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_EQ(type(S, -1), Type::kTable);
    pop(S, 1);
}

TEST_P(MetatableTest, MetatableNewAlwaysPushesMetatable)
{
    metatable_new(S, "MyMT");
    push_string(S, "field");
    push_integer(S, 42);
    table_rawset(S, -3);
    pop(S, 1);

    metatable_new(S, "MyMT");
    push_string(S, "field");
    table_rawget(S, -2);
    EXPECT_EQ(to_integer(S, -1), 42);
    pop(S, 2);
}

TEST_P(MetatableTest, MetatableFindRetrievesStoredMetatable)
{
    metatable_new(S, "TestMT");
    push_string(S, "test_field");
    push_boolean(S, true);
    table_rawset(S, -3);
    pop(S, 1);

    metatable_find(S, "TestMT");
    EXPECT_EQ(type(S, -1), Type::kTable);
    push_string(S, "test_field");
    table_rawget(S, -2);
    EXPECT_TRUE(to_boolean(S, -1));
    pop(S, 2);
}

TEST_P(MetatableTest, MetatableFindPushesNilForNonexistent)
{
    metatable_find(S, "DoesNotExist");
    EXPECT_EQ(type(S, -1), Type::kNil);
    pop(S, 1);
}

TEST_P(MetatableTest, MetatableRegistryPersistsAcrossCalls)
{
    metatable_new(S, "FileHandle");

    push_string(S, "read");
    push_cfunction(S, [](State* state) -> int {
        push_string(state, "reading...");
        return 1;
    });
    table_rawset(S, -3);
    pop(S, 1);

    metatable_find(S, "FileHandle");
    EXPECT_EQ(type(S, -1), Type::kTable);

    push_string(S, "read");
    table_rawget(S, -2);
    EXPECT_EQ(type(S, -1), Type::kCFunction);
    pop(S, 2);
}

TEST_P(MetatableTest, MetatableRegistryIsolatesStates)
{
    State* S2 = new_state();

    metatable_new(S, "StateMT");
    pop(S, 1);

    metatable_find(S2, "StateMT");
    EXPECT_EQ(type(S2, -1), Type::kNil);
    pop(S2, 1);

    close(S2);
}

TEST_P(MetatableTest, MetatableRegistryGCResistant)
{
    metatable_new(S, "GCMT");
    push_string(S, "data");
    push_integer(S, 999);
    table_rawset(S, -3);
    pop(S, 1);

    gc_collect(S);
    gc_collect(S);

    metatable_find(S, "GCMT");
    EXPECT_EQ(type(S, -1), Type::kTable);
    push_string(S, "data");
    table_rawget(S, -2);
    EXPECT_EQ(to_integer(S, -1), 999);
    pop(S, 2);
}

TEST_P(MetatableTest, MetatableNewWithUserdataPattern)
{
    constexpr uint32_t TestUD_UID = make_uid("UserData.File");
    void* ud = userdata_new(S, 16, TestUD_UID);
    ASSERT_NE(ud, nullptr);

    if (metatable_new(S, "UserData.File"))
    {
        push_string(S, "close");
        push_cfunction(S, [](State* state) -> int {
            push_string(state, "closed");
            return 1;
        });
        table_rawset(S, -3);
    }

    metatable_set(S, -2);

    EXPECT_TRUE(metatable_get(S, -1));
    push_string(S, "close");
    table_rawget(S, -2);
    EXPECT_EQ(type(S, -1), Type::kCFunction);

    pop(S, 2); // metatable and userdata
}

TEST_P(MetatableTest, GtMetamethodValueContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        return (t2 > t1) && !(t1 > t2) && !(t1 > t1)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, GeMetamethodValueContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let t3 = {value = 5}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        setmetatable(t3, mt)
        return (t2 >= t1) && (t1 >= t3) && !(t1 >= t2)
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LtMetamethodJumpContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let taken = false
        let not_taken = false
        if (t1 < t2) { taken = true }
        if (t2 < t1) { not_taken = true }
        return taken && !not_taken
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LeMetamethodJumpContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let t3 = {value = 5}
        let mt = {
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        setmetatable(t3, mt)
        let taken = false
        let equal_taken = false
        let not_taken = false
        if (t1 <= t2) { taken = true }
        if (t1 <= t3) { equal_taken = true }
        if (t2 <= t1) { not_taken = true }
        return taken && equal_taken && !not_taken
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, GtMetamethodJumpContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let taken = false
        let not_taken = false
        let equal_not_taken = false
        if (t2 > t1) { taken = true }
        if (t1 > t2) { not_taken = true }
        if (t1 > t1) { equal_not_taken = true }
        return taken && !not_taken && !equal_not_taken
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, GeMetamethodJumpContext)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let t3 = {value = 5}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        setmetatable(t3, mt)
        let taken = false
        let equal_taken = false
        let not_taken = false
        if (t2 >= t1) { taken = true }
        if (t1 >= t3) { equal_taken = true }
        if (t1 >= t2) { not_taken = true }
        return taken && equal_taken && !not_taken
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LtFamilyServesBothContextsWithoutLe)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let jump_lt = false
        let jump_ge = false
        if (t1 < t2) { jump_lt = true }
        if (t2 >= t1) { jump_ge = true }
        return (t1 < t2) && (t2 >= t1) && jump_lt && jump_ge
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, LeFamilyServesBothContextsWithoutLt)
{
    constexpr std::string_view code = R"(
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let jump_le = false
        let jump_gt = false
        if (t1 <= t2) { jump_le = true }
        if (t2 > t1) { jump_gt = true }
        return (t1 <= t2) && (t2 > t1) && jump_le && jump_gt
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(MetatableTest, ComparisonMetamethodReceivesOperandsInSourceOrder)
{
    constexpr std::string_view code = R"(
        let t1 = {name = "a"}
        let t2 = {name = "b"}
        let seen = ""
        let mt = {
            __lt = function(a, b) {
                seen = seen + a.name + b.name + ";"
                return false
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)
        let ignored = t1 < t2
        let ignored2 = t1 >= t2
        return seen
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "ab;ab;");
}

TEST_P(MetatableTest, ComparisonMetamethodsAgreeUnderJit)
{
    constexpr std::string_view code = R"(
        const jit = import("jit")
        let t1 = {value = 5}
        let t2 = {value = 10}
        let mt = {
            __lt = function(a, b) {
                return a.value < b.value
            },
            __le = function(a, b) {
                return a.value <= b.value
            }
        }
        setmetatable(t1, mt)
        setmetatable(t2, mt)

        function probe(a, b) {
            let acc = ""
            acc = acc + tostring(a < b) + tostring(a <= b)
            acc = acc + tostring(a > b) + tostring(a >= b)
            if (a < b) { acc = acc + "L" }
            if (a <= b) { acc = acc + "M" }
            if (a > b) { acc = acc + "G" }
            if (a >= b) { acc = acc + "N" }
            return acc
        }

        jit.off()
        let interpreted = probe(t1, t2)
        jit.on()
        let jitted = ""
        for (let i = 0; i < 200; i = i + 1) {
            jitted = probe(t1, t2)
        }
        return tostring(interpreted == jitted) + "|" + interpreted
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "true|truetruefalsefalseLM");
}

TEST_P(MetatableTest, GlobalReadMissUsesIndexMetamethod)
{
    constexpr std::string_view code = R"(
        let lookups = 0
        setmetatable(_G, { __index = function(t, k) { lookups = lookups + 1; return k + "!" } })
        let a = undefined_one
        let b = undefined_two
        existing = 5
        let c = existing
        return a, b, c, lookups
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::to_string(S, -4), "undefined_one!");
    ASSERT_EQ(behl::to_string(S, -3), "undefined_two!");
    ASSERT_EQ(behl::to_integer(S, -2), 5);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(MetatableTest, GlobalStrictModeRaisesOnUndefinedRead)
{
    constexpr std::string_view code = R"(
        setmetatable(_G, { __index = function(t, k) { error("undefined global " + k) } })
        function probe() { return not_defined_anywhere }
        let ok = pcall(probe)
        return ok
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, GlobalWriteOfNewKeyUsesNewIndexMetamethod)
{
    constexpr std::string_view code = R"(
        let seen = {}
        existing = 1
        setmetatable(_G, { __newindex = function(t, k, v) { seen[k] = v } })
        brand_new = 42
        existing = 2
        setmetatable(_G, nil)
        return brand_new, seen["brand_new"], existing, seen["existing"]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_TRUE(behl::is_nil(S, -4));
    ASSERT_EQ(behl::to_integer(S, -3), 42);
    ASSERT_EQ(behl::to_integer(S, -2), 2);
    ASSERT_TRUE(behl::is_nil(S, -1));
}

TEST_P(MetatableTest, GlobalIndexMetamethodInsideHotLoop)
{
    constexpr std::string_view code = R"(
        setmetatable(_G, { __index = function(t, k) { return 3 } })
        function sum(n) {
            let total = 0
            for (let i = 0; i < n; i++) {
                total = total + missing_global
            }
            return total
        }
        return sum(500)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1500);
}

TEST_P(MetatableTest, SelfReferencingIndexRaisesCatchableError)
{
    constexpr std::string_view code = R"(
        let a = {}
        setmetatable(a, a)
        a.__index = a
        let ok = pcall(function() { return a.missing })
        return ok
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, SelfReferencingNewIndexRaisesCatchableError)
{
    constexpr std::string_view code = R"(
        let b = {}
        setmetatable(b, b)
        b.__newindex = b
        let ok = pcall(function() { b.missing = 1 })
        return ok
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, EqMetamethodRemovedAfterUseFallsBackToIdentity)
{
    constexpr std::string_view code = R"(
        let mt = { __eq = function(x, y) { return true } }
        let a = setmetatable({}, mt)
        let b = setmetatable({}, mt)
        let before = a == b
        mt.__eq = nil
        let ok, after = pcall(function() { return a == b })
        return before, ok, after
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, AddMetamethodRemovedAfterUseRaisesArithmeticError)
{
    constexpr std::string_view code = R"(
        let mt = { __add = function(x, y) { return 5 } }
        let c = setmetatable({}, mt)
        let before = c + 1
        mt.__add = nil
        let ok, err = pcall(function() { return c + 1 })
        return before, ok, err
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 5);
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_NE(std::string(behl::to_string(S, -1)).find("arithmetic"), std::string::npos) << behl::to_string(S, -1);
}

TEST_P(MetatableTest, IndexMetamethodRemovedAfterUseReturnsNil)
{
    constexpr std::string_view code = R"(
        let mt = { __index = function(t, k) { return 7 } }
        let d = setmetatable({}, mt)
        let before = d.x
        mt.__index = nil
        let ok, after = pcall(function() { return d.x })
        return before, ok, after
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 7);
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(MetatableTest, LtMetamethodRemovedAfterUseRaisesCompareError)
{
    constexpr std::string_view code = R"(
        let mt = { __lt = function(x, y) { return true } }
        let e = setmetatable({}, mt)
        let f = setmetatable({}, mt)
        let before = e < f
        mt.__lt = nil
        let ok, err = pcall(function() { return e < f })
        return before, ok, err
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_NE(std::string(behl::to_string(S, -1)).find("compare"), std::string::npos) << behl::to_string(S, -1);
}

TEST_P(MetatableTest, LenMetamethodRemovedAfterUseFallsBackToRawlen)
{
    constexpr std::string_view code = R"(
        let mt = { __len = function(x) { return 9 } }
        let g = setmetatable({1, 2}, mt)
        let before = #g
        mt.__len = nil
        let ok, after = pcall(function() { return #g })
        return before, ok, after
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 9);
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(MetatableTest, CallMetamethodRemovedAfterUseRaisesCallError)
{
    constexpr std::string_view code = R"(
        let mt = { __call = function(x) { return 3 } }
        let h = setmetatable({}, mt)
        let before = h()
        mt.__call = nil
        let ok = pcall(function() { return h() })
        return before, ok
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, TostringMetamethodRemovedAfterUseUsesDefault)
{
    constexpr std::string_view code = R"(
        let mt = { __tostring = function(x) { return "T" } }
        let i = setmetatable({}, mt)
        let before = tostring(i)
        mt.__tostring = nil
        let ok, after = pcall(function() { return tostring(i) })
        return before, ok, typeof(after), after != "T"
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_string(S, -4), "T");
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_EQ(behl::to_string(S, -2), "string");
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, EqualityOfSameTableDoesNotCallEq)
{
    constexpr std::string_view code = R"(
        let calls = 0
        let mt = { __eq = function(x, y) { calls++; return false } }
        let a = setmetatable({}, mt)
        let same = a == a
        let different = true
        if (a != a) { different = true } else { different = false }
        return same, different, calls
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(MetatableTest, EqMetamethodReturningNilIsFalse)
{
    constexpr std::string_view code = R"(
        let mt = { __eq = function(x, y) { return nil } }
        let p = setmetatable({}, mt)
        let q = setmetatable({}, mt)
        return p == q
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, EqMetamethodInBranchContextHonoursResult)
{
    constexpr std::string_view code = R"(
        let mf = { __eq = function(x, y) { return false } }
        let a = setmetatable({}, mf)
        let b = setmetatable({}, mf)
        let mn = { __eq = function(x, y) { return nil } }
        let p = setmetatable({}, mn)
        let q = setmetatable({}, mn)
        let mt = { __eq = function(x, y) { return true } }
        let c = setmetatable({}, mt)
        let d = setmetatable({}, mt)
        let log = ""
        if (a == b) { log = log + "ab:eq " } else { log = log + "ab:ne " }
        if (p == q) { log = log + "pq:eq " } else { log = log + "pq:ne " }
        if (c == d) { log = log + "cd:eq" } else { log = log + "cd:ne" }
        return log
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "ab:ne pq:ne cd:eq");
}

TEST_P(MetatableTest, NotEqualInValueContextNegatesEqMetamethod)
{
    constexpr std::string_view code = R"(
        let mf = { __eq = function(x, y) { return false } }
        let a = setmetatable({}, mf)
        let b = setmetatable({}, mf)
        let mn = { __eq = function(x, y) { return nil } }
        let p = setmetatable({}, mn)
        let q = setmetatable({}, mn)
        let mt = { __eq = function(x, y) { return true } }
        let c = setmetatable({}, mt)
        let d = setmetatable({}, mt)
        let r1 = a != b
        let r2 = p != q
        let r3 = c != d
        return r1, r2, r3
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(MetatableTest, EqMetamethodNotUsedForTableKeyLookup)
{
    constexpr std::string_view code = R"(
        let mt = { __eq = function(x, y) { return true } }
        let k1 = setmetatable({}, mt)
        let k2 = setmetatable({}, mt)
        let t = {}
        t[k1] = "one"
        return k1 == k2, t[k2], t[k1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "one");
}

TEST_P(MetatableTest, PairsMetamethodDrivesForeachAndPairs)
{
    constexpr std::string_view code = R"(
        let t = {}
        setmetatable(t, { __pairs = function(self) {
            let i = -1
            return function(s, k) {
                i++
                if (i < 3) { return i, i * 10 }
                return nil
            }, self, nil
        } })
        let n = 0
        let s = 0
        foreach (let k, v in t) {
            n++
            s = s + v
            if (n > 100) { break }
        }
        let s2 = 0
        let n2 = 0
        for (let k, v in pairs(t)) {
            n2++
            s2 = s2 + v
            if (n2 > 100) { break }
        }
        return n, s, s2
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), 30);
    EXPECT_EQ(behl::to_integer(S, -1), 30);
}

TEST_P(MetatableTest, NewIndexPointingAtOwnMetatableStoresInMetatable)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let mt = {}
        mt.__newindex = mt
        let o = setmetatable({}, mt)
        o.x = 5
        return table.rawget(o, "x"), table.rawget(mt, "x"), o.x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::is_nil(S, -3));
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(MetatableTest, TableAsItsOwnMetatable)
{
    constexpr std::string_view code = R"(
        let st = {}
        setmetatable(st, st)
        st.__index = { y = 9 }
        return getmetatable(st) == st, st.y, st.zz
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_EQ(behl::to_integer(S, -2), 9);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(MetatableTest, NewIndexChainReachesGrandparent)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let gp = {}
        let par = setmetatable({}, { __newindex = gp })
        let ch = setmetatable({}, { __newindex = par })
        ch.v = 42
        return table.rawget(ch, "v"), table.rawget(par, "v"), gp.v
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_TRUE(behl::is_nil(S, -3));
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(MetatableTest, ProxyUsesSameTableForIndexAndNewIndex)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let store = {}
        let proxy = setmetatable({}, { __index = store, __newindex = store })
        proxy.a = 1
        proxy.b = 2
        proxy.a = 3
        let count = 0
        for (let k, v in pairs(proxy)) {
            count++
            if (count > 100) { break }
        }
        return proxy.a, proxy.b, store.a, store.b, count, table.rawget(proxy, "a")
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 6));
    EXPECT_EQ(behl::to_integer(S, -6), 3);
    EXPECT_EQ(behl::to_integer(S, -5), 2);
    EXPECT_EQ(behl::to_integer(S, -4), 3);
    EXPECT_EQ(behl::to_integer(S, -3), 2);
    EXPECT_EQ(behl::to_integer(S, -2), 0);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(MetatableTest, ThreeLevelIndexChainEndsInFunction)
{
    constexpr std::string_view code = R"(
        let base = setmetatable({}, { __index = function(t, k) { return "fn:" + k } })
        let mid = setmetatable({}, { __index = base })
        let top = setmetatable({}, { __index = mid })
        mid.m = "mid"
        return top.m, top.zz
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_string(S, -2), "mid");
    EXPECT_EQ(behl::to_string(S, -1), "fn:zz");
}

TEST_P(MetatableTest, ComparisonBetweenNumberAndTableKeepsOperandOrder)
{
    constexpr std::string_view code = R"behl(
        let log = ""
        let mt = {
            __lt = function(a, b) {
                log = log + "lt(" + typeof(a) + "," + typeof(b) + ")"
                return typeof(a) == "integer"
            },
            __le = function(a, b) {
                log = log + "le(" + typeof(a) + "," + typeof(b) + ")"
                return typeof(a) == "integer"
            }
        }
        let t = setmetatable({}, mt)
        let r1 = 1 < t
        let r2 = t >= 1
        let r3 = 1 <= t
        let r4 = t > 1
        let r5 = 1 > t
        let r6 = 1 >= t
        return r1, r2, r3, r4, r5, r6, log
    )behl";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 7));
    EXPECT_TRUE(behl::to_boolean(S, -7));
    EXPECT_TRUE(behl::to_boolean(S, -6));
    EXPECT_TRUE(behl::to_boolean(S, -5));
    EXPECT_TRUE(behl::to_boolean(S, -4));
    EXPECT_FALSE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_string(S, -1),
        "lt(integer,table)lt(table,integer)le(integer,table)le(table,integer)le(integer,table)lt(integer,table)");
}

TEST_P(MetatableTest, ArithmeticMetamethodsKeepOperandOrderWithNumberOnLeft)
{
    constexpr std::string_view code = R"(
        let mt = {
            __add = function(a, b) { return typeof(a) + "+" + typeof(b) },
            __sub = function(a, b) { return typeof(a) + "-" + typeof(b) },
            __mul = function(a, b) { return typeof(a) + "*" + typeof(b) },
            __div = function(a, b) { return typeof(a) + "/" + typeof(b) },
            __mod = function(a, b) { return typeof(a) + "%" + typeof(b) },
            __pow = function(a, b) { return typeof(a) + "**" + typeof(b) }
        }
        let x = setmetatable({}, mt)
        let left = (1 + x) + " " + (1 - x) + " " + (1 * x) + " " + (1 / x) + " " + (1 % x) + " " + (2 ** x)
        let right = (x + 1) + " " + (x - 1) + " " + (x * 1) + " " + (x / 1) + " " + (x % 1) + " " + (x ** 2)
        return left, right
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_string(S, -2),
        "integer+table integer-table integer*table integer/table integer%table integer**table");
    EXPECT_EQ(behl::to_string(S, -1),
        "table+integer table-integer table*integer table/integer table%integer table**integer");
}

TEST_P(MetatableTest, BitwiseMetamethodsKeepOperandOrderWithNumberOnLeft)
{
    constexpr std::string_view code = R"(
        let mt = {
            __band = function(a, b) { return typeof(a) + "&" + typeof(b) },
            __bor = function(a, b) { return typeof(a) + "|" + typeof(b) },
            __bxor = function(a, b) { return typeof(a) + "^" + typeof(b) },
            __shl = function(a, b) { return typeof(a) + "<<" + typeof(b) },
            __shr = function(a, b) { return typeof(a) + ">>" + typeof(b) }
        }
        let x = setmetatable({}, mt)
        let left = (1 & x) + " " + (1 | x) + " " + (1 ^ x) + " " + (1 << x) + " " + (1 >> x)
        let right = (x & 1) + " " + (x | 1) + " " + (x ^ 1) + " " + (x << 1) + " " + (x >> 1)
        return left, right
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_string(S, -2), "integer&table integer|table integer^table integer<<table integer>>table");
    EXPECT_EQ(behl::to_string(S, -1), "table&integer table|integer table^integer table<<integer table>>integer");
}

TEST_P(MetatableTest, LenMetamethodNonIntegerResultIsReturnedAsIs)
{
    constexpr std::string_view code = R"(
        let lt = setmetatable({}, { __len = function(t) { return 2.5 } })
        let ls = setmetatable({}, { __len = function(t) { return "abc" } })
        return #lt, typeof(#lt), #ls
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -3), 2.5);
    EXPECT_EQ(behl::to_string(S, -2), "number");
    EXPECT_EQ(behl::to_string(S, -1), "abc");
}

TEST_P(MetatableTest, RawlenOfNonTableIsZero)
{
    constexpr std::string_view code = R"(
        return rawlen("abc") + rawlen(5) + rawlen(nil) + rawlen(true)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(MetatableTest, GetmetatableOfNonTableIsNil)
{
    constexpr std::string_view code = R"(
        return getmetatable(5) == nil, getmetatable(nil) == nil, getmetatable(true) == nil, getmetatable(print) == nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_TRUE(behl::to_boolean(S, -4));
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

INSTANTIATE_TEST_SUITE_P(Mode, MetatableTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
