#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <cstring>
#include <gtest/gtest.h>

struct TestData
{
    int value;
    double score;
    char name[32];
};

constexpr uint32_t TestData_UID = behl::make_uid("TestData");

static int create_test_userdata(behl::State* S)
{
    void* ptr = behl::userdata_new(S, sizeof(TestData), TestData_UID);
    TestData* data = static_cast<TestData*>(ptr);
    data->value = 42;
    data->score = 3.14;
#ifdef _MSC_VER
    strcpy_s(data->name, sizeof(data->name), "test");
#else
    std::strcpy(data->name, "test");
#endif
    return 1;
}

static int get_userdata_value(behl::State* S)
{
    TestData* data = static_cast<TestData*>(behl::check_userdata(S, 0, TestData_UID));
    behl::push_integer(S, data->value);
    return 1;
}

static int set_userdata_value(behl::State* S)
{
    TestData* data = static_cast<TestData*>(behl::check_userdata(S, 0, TestData_UID));
    data->value = static_cast<int>(behl::to_integer(S, 1));
    return 0;
}

static int gc_counter = 0;
static int finalizer_counter(behl::State* S)
{
    TestData* data = static_cast<TestData*>(behl::to_userdata(S, 0));
    if (data)
    {
        gc_counter++;
    }
    return 0;
}

static int finalizer_modify(behl::State* S)
{
    TestData* data = static_cast<TestData*>(behl::to_userdata(S, 0));
    if (data)
    {
        data->value = 999;
    }
    return 0;
}

class UserdataTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
        behl::register_function(S, "create_test_userdata", create_test_userdata);
        behl::register_function(S, "get_userdata_value", get_userdata_value);
        behl::register_function(S, "set_userdata_value", set_userdata_value);
        behl::register_function(S, "finalizer_counter", finalizer_counter);
        behl::register_function(S, "finalizer_modify", finalizer_modify);
        gc_counter = 0;
    }

    void TearDown() override
    {
        behl::close(S);
    }

    bool run_code(std::string_view code)
    {
        try
        {
            behl::load_string(S, code);
            behl::call(S, 0, 0);
            return true;
        }
        catch (const std::exception& e)
        {
            EXPECT_TRUE(false) << "Error running code: " << e.what();
            return false;
        }
    }
};

TEST_F(UserdataTest, CreateUserdataFromCAPI)
{
    void* data = behl::userdata_new(S, sizeof(TestData), TestData_UID);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(behl::type(S, -1), behl::Type::kUserdata);

    TestData* td = static_cast<TestData*>(data);
    td->value = 100;
    td->score = 2.5;

    TestData* retrieved = static_cast<TestData*>(behl::to_userdata(S, -1));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->value, 100);
    EXPECT_DOUBLE_EQ(retrieved->score, 2.5);
}

TEST_F(UserdataTest, CreateUserdataFromScript)
{
    constexpr std::string_view code = R"(
        let ud = create_test_userdata();
        let val = get_userdata_value(ud);
    )";

    EXPECT_TRUE(run_code(code));

    ASSERT_NO_THROW(behl::load_string(S, "let ud = create_test_userdata(); return get_userdata_value(ud);"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, UserdataTypeCheck)
{
    ASSERT_NO_THROW(behl::load_string(S, "let ud = create_test_userdata(); return typeof(ud);"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "userdata");
}

TEST_F(UserdataTest, ModifyUserdataValue)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        set_userdata_value(ud, 123);
        return get_userdata_value(ud);
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 123);
}

TEST_F(UserdataTest, UserdataInTable)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let t = {};
        t["userdata"] = create_test_userdata();
        return get_userdata_value(t["userdata"]);
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, UserdataAsTableKey)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let t = {};
        t[ud] = "stored_value";
        return t[ud];
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "stored_value");
}

TEST_F(UserdataTest, UserdataWithMetatable)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let mt = {};
        setmetatable(ud, mt);
        return getmetatable(ud) != nil;
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataMetatableIndex)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let mt = {};
        mt["__index"] = function(t, k) {
            if (k == "value") {
                return get_userdata_value(t);
            }
            return nil;
        };
        setmetatable(ud, mt);
        return ud["value"];
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, UserdataMetatableNewIndex)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let mt = {};
        let storage = {};
        mt["__newindex"] = function(t, k, v) {
            storage[k] = v;
        };
        setmetatable(ud, mt);
        ud["custom_field"] = 999;
        return storage["custom_field"];
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 999);
}

TEST_F(UserdataTest, UserdataMetatableAdd)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud1 = create_test_userdata();
        let ud2 = create_test_userdata();
        let mt = {};
        mt["__add"] = function(a, b) {
            return get_userdata_value(a) + get_userdata_value(b);
        };
        setmetatable(ud1, mt);
        setmetatable(ud2, mt);
        return ud1 + ud2;
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 84);
}

TEST_F(UserdataTest, UserdataMetatableCall)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let mt = {};
        mt["__call"] = function(self, arg) {
            return get_userdata_value(self) + arg;
        };
        setmetatable(ud, mt);
        let result = ud(10);
        return result == 52
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataMetatableTailCall)
{
    ASSERT_NO_THROW(behl::load_string(S, R"(
        let ud = create_test_userdata();
        let mt = {};
        mt["__call"] = function(self, arg) {
            return get_userdata_value(self) + arg;
        };
        setmetatable(ud, mt);
        return ud(10);
    )"));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 52);
}

TEST_F(UserdataTest, UserdataMetatableToString)
{
    constexpr std::string_view code = R"(
        let ud = create_test_userdata();
        let mt = {};
        mt.__tostring = function(self) {
            let val = get_userdata_value(self);
            return 'TestData(' + tostring(val) + ')';
        };
        setmetatable(ud, mt);
        
        let str = tostring(ud);
        return str;
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "TestData(42)");
}

TEST_F(UserdataTest, UserdataGCFinalizerCalled)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        let ud = create_test_userdata();
        setmetatable(ud, mt);
        ud = nil;
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
    EXPECT_EQ(gc_counter, 1);
}

TEST_F(UserdataTest, UserdataGCMultipleFinalizersCalled)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        for (let i = 0; i < 10; i++) {
            let ud = create_test_userdata();
            setmetatable(ud, mt);
        }
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
    EXPECT_EQ(gc_counter, 10);
}

TEST_F(UserdataTest, UserdataGCFinalizerModifiesData)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_modify;
        let ud = create_test_userdata();
        setmetatable(ud, mt);
        ud = nil;
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
}

TEST_F(UserdataTest, UserdataWithoutFinalizerNoError)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let ud = create_test_userdata();
        ud = nil;
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
}

TEST_F(UserdataTest, UserdataReferencedNotCollected)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        
        global_ud = create_test_userdata();
        setmetatable(global_ud, mt);
        
        gc.collect();
        
        return get_userdata_value(global_ud);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    EXPECT_EQ(gc_counter, 0); // Should not be finalized

    EXPECT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(UserdataTest, UserdataInClosurePreserved)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        
        function make_closure() {
            let ud = create_test_userdata();
            setmetatable(ud, mt);
            
            return function() {
                return get_userdata_value(ud);
            };
        }
        
        closure = make_closure();
        gc.collect();
        
        return closure();
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    EXPECT_EQ(gc_counter, 0); // Should not be finalized yet
    EXPECT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(UserdataTest, UserdataPassedToFunction)
{
    constexpr std::string_view code = R"(
        function process_userdata(ud, multiplier) {
            return get_userdata_value(ud) * multiplier;
        }
        
        let ud = create_test_userdata();
        return process_userdata(ud, 2);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 84);
}

TEST_F(UserdataTest, UserdataReturnedFromFunction)
{
    constexpr std::string_view code = R"(
        function create_wrapper() {
            return create_test_userdata();
        }
        
        let ud = create_wrapper();
        return get_userdata_value(ud);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, MultipleUserdataIndependent)
{
    constexpr std::string_view code = R"(
        let ud1 = create_test_userdata();
        let ud2 = create_test_userdata();
        
        set_userdata_value(ud1, 100);
        set_userdata_value(ud2, 200);
        
        return get_userdata_value(ud1) == 100 && get_userdata_value(ud2) == 200;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataArrayInTable)
{
    constexpr std::string_view code = R"(
        let arr = {};
        for (let i = 0; i < 5; i++) {
            arr[i] = create_test_userdata();
        }
        
        let all_correct = true;
        for (let i = 0; i < 5; i++) {
            let val = get_userdata_value(arr[i]);
            if (val != 42) {
                all_correct = false;
            }
        }
        return all_correct;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataWithNestedMetatables)
{
    constexpr std::string_view code = R"(
        let ud = create_test_userdata();
        
        let parent_mt = {};
        parent_mt["get_value"] = function(self) {
            return get_userdata_value(self);
        };
        
        let child_mt = {};
        child_mt["__index"] = parent_mt;
        
        setmetatable(ud, child_mt);
        
        return ud["get_value"](ud);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, UserdataMetatableEq)
{
    constexpr std::string_view code = R"(
        let ud1 = create_test_userdata();
        let ud2 = create_test_userdata();
        
        let mt = {};
        mt["__eq"] = function(a, b) {
            return get_userdata_value(a) == get_userdata_value(b);
        };
        
        setmetatable(ud1, mt);
        setmetatable(ud2, mt);
        
        return ud1 == ud2;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataMetatableLt)
{
    constexpr std::string_view code = R"(
        let ud1 = create_test_userdata();
        let ud2 = create_test_userdata();
        
        set_userdata_value(ud1, 10);
        set_userdata_value(ud2, 20);
        
        let mt = {};
        mt["__lt"] = function(a, b) {
            return get_userdata_value(a) < get_userdata_value(b);
        };
        
        setmetatable(ud1, mt);
        setmetatable(ud2, mt);
        
        let is_less = ud1 < ud2;
        let is_not_less = ud2 < ud1;
        return is_less && !is_not_less;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataMetatableLen)
{
    constexpr std::string_view code = R"(
        let ud = create_test_userdata();
        
        let mt = {};
        mt["__len"] = function(self) {
            return get_userdata_value(self);
        };
        
        setmetatable(ud, mt);
        
        return #ud;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(UserdataTest, UserdataGCWithTableReferences)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        
        let container = {};
        
        for (let i = 0; i < 5; i++) {
            let ud = create_test_userdata();
            setmetatable(ud, mt);
            container[i] = ud;
        }
        
        container = nil;
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
    EXPECT_EQ(gc_counter, 5);
}

TEST_F(UserdataTest, UserdataStressTestManyAllocations)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        
        for (let i = 0; i < 100; i++) {
            let ud = create_test_userdata();
            setmetatable(ud, mt);
        }
        
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
    EXPECT_EQ(gc_counter, 100);
}

TEST_F(UserdataTest, UserdataCircularReferenceWithTable)
{
    constexpr std::string_view code = R"(
        const gc = import("gc");
        let mt = {};
        mt["__gc"] = finalizer_counter;
        mt["__index"] = mt;
        
        let ud = create_test_userdata();
        setmetatable(ud, mt);
        
        let t = {};
        t["userdata"] = ud;
        mt["table"] = t;
        
        ud = nil;
        t = nil;
        mt = nil;
        
        gc.collect();
    )";

    EXPECT_TRUE(run_code(code));
    EXPECT_EQ(gc_counter, 1); // Should still be collected
}

TEST_F(UserdataTest, ToUserdataReturnsNullForNonUserdata)
{
    behl::push_integer(S, 42);
    void* result = behl::to_userdata(S, -1);
    EXPECT_EQ(result, nullptr);
}

TEST_F(UserdataTest, CheckUserdataThrowsForNonUserdata)
{
    constexpr std::string_view code = R"(
        get_userdata_value(42);
    )";

    EXPECT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0)); // Should fail with type error
}

TEST_F(UserdataTest, UserdataZeroSize)
{
    void* data = behl::userdata_new(S, 0, TestData_UID);
    EXPECT_NE(data, nullptr); // Should still return valid pointer
    EXPECT_EQ(behl::type(S, -1), behl::Type::kUserdata);
}

TEST_F(UserdataTest, CheckUserdataWrongUIDThrows)
{
    constexpr uint32_t WrongUID = behl::make_uid("WrongType");

    void* data = behl::userdata_new(S, sizeof(TestData), TestData_UID);
    ASSERT_NE(data, nullptr);

    EXPECT_THROW({ behl::check_userdata(S, -1, WrongUID); }, behl::RuntimeError);
}

TEST_F(UserdataTest, CheckUserdataCorrectUIDSucceeds)
{
    void* data = behl::userdata_new(S, sizeof(TestData), TestData_UID);
    ASSERT_NE(data, nullptr);

    void* checked = nullptr;
    EXPECT_NO_THROW({ checked = behl::check_userdata(S, -1, TestData_UID); });
    EXPECT_EQ(data, checked);
}

TEST_F(UserdataTest, MixedUserdataTypesIsolated)
{
    constexpr uint32_t TypeA_UID = behl::make_uid("TypeA");
    constexpr uint32_t TypeB_UID = behl::make_uid("TypeB");

    void* dataA = behl::userdata_new(S, 16, TypeA_UID);
    void* dataB = behl::userdata_new(S, 32, TypeB_UID);

    ASSERT_NE(dataA, nullptr);
    ASSERT_NE(dataB, nullptr);

    void* checkedB = nullptr;
    EXPECT_NO_THROW({ checkedB = behl::check_userdata(S, -1, TypeB_UID); });
    EXPECT_EQ(dataB, checkedB);

    EXPECT_THROW({ behl::check_userdata(S, -1, TypeA_UID); }, behl::RuntimeError);

    void* checkedA = nullptr;
    EXPECT_NO_THROW({ checkedA = behl::check_userdata(S, -2, TypeA_UID); });
    EXPECT_EQ(dataA, checkedA);

    EXPECT_THROW({ behl::check_userdata(S, -2, TypeB_UID); }, behl::RuntimeError);
}

TEST_F(UserdataTest, UserdataUIDMismatchFromScript)
{
    behl::push_cfunction(S, [](behl::State* state) -> int {
        constexpr uint32_t MyType_UID = behl::make_uid("MyType");
        void* data = behl::userdata_new(state, 8, MyType_UID);
        *static_cast<int*>(data) = 42;
        return 1;
    });
    behl::set_global(S, "create_my_type");

    behl::push_cfunction(S, [](behl::State* state) -> int {
        constexpr uint32_t WrongType_UID = behl::make_uid("WrongType");
        void* data = behl::check_userdata(state, 0, WrongType_UID);
        behl::push_integer(state, *static_cast<int*>(data));
        return 1;
    });
    behl::set_global(S, "read_wrong_type");

    constexpr std::string_view code = R"(
        let obj = create_my_type();
        let result = pcall(function() {
            return read_wrong_type(obj);
        });
        return result;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(UserdataTest, UserdataGetUID)
{
    constexpr uint32_t TypeA_UID = behl::make_uid("TypeA");
    constexpr uint32_t TypeB_UID = behl::make_uid("TypeB");

    void* dataA = behl::userdata_new(S, 8, TypeA_UID);
    ASSERT_NE(dataA, nullptr);

    uint32_t retrieved_uid = behl::userdata_get_uid(S, -1);
    EXPECT_EQ(retrieved_uid, TypeA_UID);

    void* dataB = behl::userdata_new(S, 16, TypeB_UID);
    ASSERT_NE(dataB, nullptr);

    retrieved_uid = behl::userdata_get_uid(S, -1);
    EXPECT_EQ(retrieved_uid, TypeB_UID);

    retrieved_uid = behl::userdata_get_uid(S, -2);
    EXPECT_EQ(retrieved_uid, TypeA_UID);

    behl::push_integer(S, 42);
    retrieved_uid = behl::userdata_get_uid(S, -1);
    EXPECT_EQ(retrieved_uid, 0);
}

TEST_F(UserdataTest, UserdataPolymorphicHandler)
{
    behl::push_cfunction(S, [](behl::State* state) -> int {
        constexpr uint32_t TypeA_UID = behl::make_uid("PolyTypeA");
        constexpr uint32_t TypeB_UID = behl::make_uid("PolyTypeB");

        uint32_t uid = behl::userdata_get_uid(state, 0);

        if (uid == TypeA_UID)
        {
            void* data = behl::check_userdata(state, 0, TypeA_UID);
            int* value = static_cast<int*>(data);
            behl::push_string(state, "TypeA");
            behl::push_integer(state, *value);
            return 2;
        }
        else if (uid == TypeB_UID)
        {
            void* data = behl::check_userdata(state, 0, TypeB_UID);
            double* value = static_cast<double*>(data);
            behl::push_string(state, "TypeB");
            behl::push_number(state, *value);
            return 2;
        }
        else
        {
            behl::error(state, "Unknown userdata type");
        }
    });
    behl::set_global(S, "handle_poly");

    behl::push_cfunction(S, [](behl::State* state) -> int {
        constexpr uint32_t TypeA_UID = behl::make_uid("PolyTypeA");
        void* data = behl::userdata_new(state, sizeof(int), TypeA_UID);
        *static_cast<int*>(data) = 100;
        return 1;
    });
    behl::set_global(S, "create_typeA");

    behl::push_cfunction(S, [](behl::State* state) -> int {
        constexpr uint32_t TypeB_UID = behl::make_uid("PolyTypeB");
        void* data = behl::userdata_new(state, sizeof(double), TypeB_UID);
        *static_cast<double*>(data) = 3.14;
        return 1;
    });
    behl::set_global(S, "create_typeB");

    constexpr std::string_view code = R"(
        let objA = create_typeA();
        let objB = create_typeB();
        
        let typeA_name, typeA_val = handle_poly(objA);
        let typeB_name, typeB_val = handle_poly(objB);
        
        return typeA_name, typeA_val, typeB_name, typeB_val;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));

    EXPECT_EQ(behl::to_string(S, -4), "TypeA");
    EXPECT_EQ(behl::to_integer(S, -3), 100);
    EXPECT_EQ(behl::to_string(S, -2), "TypeB");
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 3.14);
}
