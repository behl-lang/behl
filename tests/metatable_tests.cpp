#include "common/string.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
using namespace behl;

class MetatableTest : public ::testing::Test
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

TEST_F(MetatableTest, GetMetatableReturnsNilForNoMetatable)
{
    constexpr std::string_view code = R"(
        let t = {a = 1}
        return getmetatable(t) == nil
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_F(MetatableTest, SetAndGetMetatable)
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

TEST_F(MetatableTest, SetMetatableReturnsTable)
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

TEST_F(MetatableTest, SetMetatableToNilRemovesMetatable)
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

TEST_F(MetatableTest, IndexMetamethodWithFunction)
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

TEST_F(MetatableTest, IndexMetamethodWithTable)
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

TEST_F(MetatableTest, IndexMetamethodChaining)
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

TEST_F(MetatableTest, IndexMetamethodNotCalledForExistingKey)
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

TEST_F(MetatableTest, NewIndexMetamethodWithFunction)
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

TEST_F(MetatableTest, NewIndexMetamethodWithTable)
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

TEST_F(MetatableTest, NewIndexMetamethodNotCalledForExistingKey)
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

TEST_F(MetatableTest, AddMetamethod)
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

TEST_F(MetatableTest, SubMetamethod)
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

TEST_F(MetatableTest, MulMetamethod)
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

TEST_F(MetatableTest, DivMetamethod)
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

TEST_F(MetatableTest, ModMetamethod)
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

TEST_F(MetatableTest, PowMetamethod)
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

TEST_F(MetatableTest, UnmMetamethod)
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

TEST_F(MetatableTest, ArithmeticWithMixedTypes)
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

TEST_F(MetatableTest, EqMetamethod)
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

TEST_F(MetatableTest, LtMetamethod)
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

TEST_F(MetatableTest, LeMetamethod)
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

TEST_F(MetatableTest, CallMetamethod)
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

TEST_F(MetatableTest, CallMetamethodWithMultipleArgs)
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

TEST_F(MetatableTest, ToStringMetamethod)
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

TEST_F(MetatableTest, LenMetamethod)
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

TEST_F(MetatableTest, RawGetBypassesMetatable)
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

TEST_F(MetatableTest, RawSetBypassesMetatable)
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

TEST_F(MetatableTest, RawLenBypassesMetatable)
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

TEST_F(MetatableTest, SimpleClassPattern)
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

TEST_F(MetatableTest, InheritancePattern)
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

TEST_F(MetatableTest, MetatableOnMetatable)
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

TEST_F(MetatableTest, MetamethodReturnsMultipleValues)
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

TEST_F(MetatableTest, RecursiveIndexLookup)
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

TEST_F(MetatableTest, ArithmeticMetamethodOnlyOneOperand)
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

TEST_F(MetatableTest, ComparisonRequiresBothMetamethods)
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

TEST_F(MetatableTest, DeepNestedAddMetamethod32Levels)
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

TEST_F(MetatableTest, DeepNestedMultipleArithmeticOps)
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

TEST_F(MetatableTest, DeepNestedWithTableCreation)
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

TEST_F(MetatableTest, DeepNestedChainedMetamethods)
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

TEST_F(MetatableTest, ExtremeMixedOperationsDepth64)
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

TEST_F(MetatableTest, DeepNestedWithUpvalues)
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

TEST_F(MetatableTest, StressTestMassiveStackGrowth)
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

TEST_F(MetatableTest, BitwiseAndMetamethod)
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

TEST_F(MetatableTest, BitwiseOrMetamethod)
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

TEST_F(MetatableTest, BitwiseXorMetamethod)
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

TEST_F(MetatableTest, BitwiseLeftShiftMetamethod)
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

TEST_F(MetatableTest, BitwiseRightShiftMetamethod)
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

TEST_F(MetatableTest, BitwiseNotMetamethod)
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

TEST_F(MetatableTest, BitwiseMixedOperations)
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

TEST_F(MetatableTest, BitwiseDeepNested)
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

TEST_F(MetatableTest, BitwiseWithUpvalues)
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

TEST_F(MetatableTest, MetatableNewCreatesNewMetatable)
{
    bool created = metatable_new(S, "TestMetatable");
    EXPECT_TRUE(created);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_EQ(type(S, -1), Type::kTable);
    pop(S, 1);
}

TEST_F(MetatableTest, MetatableNewReturnsFalseForExisting)
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

TEST_F(MetatableTest, MetatableNewAlwaysPushesMetatable)
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

TEST_F(MetatableTest, MetatableFindRetrievesStoredMetatable)
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

TEST_F(MetatableTest, MetatableFindPushesNilForNonexistent)
{
    metatable_find(S, "DoesNotExist");
    EXPECT_EQ(type(S, -1), Type::kNil);
    pop(S, 1);
}

TEST_F(MetatableTest, MetatableRegistryPersistsAcrossCalls)
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

TEST_F(MetatableTest, MetatableRegistryIsolatesStates)
{
    State* S2 = new_state();

    metatable_new(S, "StateMT");
    pop(S, 1);

    metatable_find(S2, "StateMT");
    EXPECT_EQ(type(S2, -1), Type::kNil);
    pop(S2, 1);

    close(S2);
}

TEST_F(MetatableTest, MetatableRegistryGCResistant)
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

TEST_F(MetatableTest, MetatableNewWithUserdataPattern)
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
