#include "gc/gc_list.hpp"

#include <gtest/gtest.h>

using namespace behl;

struct MockGCObject : GCObject
{
    int id;
    explicit MockGCObject(int id_val)
        : GCObject(GCType::kDead)
        , id(id_val)
    {
        next = nullptr;
        prev = nullptr;
    }
};

class GCListTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        for (int i = 0; i < 10; ++i)
        {
            objects[i] = new MockGCObject(i);
        }
    }

    void TearDown() override
    {
        for (auto* obj : objects)
        {
            delete obj;
        }
    }

    MockGCObject* objects[10];
};

TEST_F(GCListTest, EmptyListIsValid)
{
    GCList list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.count(), 0);
    EXPECT_EQ(list.head(), nullptr);
    EXPECT_EQ(list.tail(), nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, AppendSingleObject)
{
    GCList list;
    list.append(objects[0]);

    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.count(), 1);
    EXPECT_EQ(list.head(), objects[0]);
    EXPECT_EQ(list.tail(), objects[0]);
    EXPECT_EQ(objects[0]->prev, nullptr);
    EXPECT_EQ(objects[0]->next, nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, AppendMultipleObjects)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    EXPECT_EQ(list.count(), 5);
    EXPECT_EQ(list.head(), objects[0]);
    EXPECT_EQ(list.tail(), objects[4]);
    EXPECT_TRUE(list.validate());

    EXPECT_EQ(objects[0]->next, objects[1]);
    EXPECT_EQ(objects[1]->next, objects[2]);
    EXPECT_EQ(objects[2]->next, objects[3]);
    EXPECT_EQ(objects[3]->next, objects[4]);
    EXPECT_EQ(objects[4]->next, nullptr);

    EXPECT_EQ(objects[0]->prev, nullptr);
    EXPECT_EQ(objects[1]->prev, objects[0]);
    EXPECT_EQ(objects[2]->prev, objects[1]);
    EXPECT_EQ(objects[3]->prev, objects[2]);
    EXPECT_EQ(objects[4]->prev, objects[3]);
}

TEST_F(GCListTest, FirstObjectStaysHead)
{
    GCList list;

    list.append(objects[0]);
    EXPECT_EQ(list.head(), objects[0]);

    for (int i = 1; i < 5; ++i)
    {
        list.append(objects[i]);
        EXPECT_EQ(list.head(), objects[0]);
    }

    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveMiddleObject)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    list.remove(objects[2]);

    EXPECT_EQ(list.count(), 4);
    EXPECT_EQ(objects[1]->next, objects[3]);
    EXPECT_EQ(objects[3]->prev, objects[1]);
    EXPECT_EQ(objects[2]->next, nullptr);
    EXPECT_EQ(objects[2]->prev, nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveHeadObject)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    list.remove(objects[0]);

    EXPECT_EQ(list.count(), 4);
    EXPECT_EQ(list.head(), objects[1]);
    EXPECT_EQ(objects[1]->prev, nullptr);
    EXPECT_EQ(objects[0]->next, nullptr);
    EXPECT_EQ(objects[0]->prev, nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveTailObject)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    list.remove(objects[4]);

    EXPECT_EQ(list.count(), 4);
    EXPECT_EQ(list.tail(), objects[3]);
    EXPECT_EQ(objects[3]->next, nullptr);
    EXPECT_EQ(objects[4]->next, nullptr);
    EXPECT_EQ(objects[4]->prev, nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveOnlyObject)
{
    GCList list;
    list.append(objects[0]);

    list.remove(objects[0]);

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.count(), 0);
    EXPECT_EQ(list.head(), nullptr);
    EXPECT_EQ(list.tail(), nullptr);
    EXPECT_EQ(objects[0]->next, nullptr);
    EXPECT_EQ(objects[0]->prev, nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveAllObjects)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    for (int i = 0; i < 5; ++i)
    {
        list.remove(objects[i]);
        EXPECT_TRUE(list.validate());
    }

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.count(), 0);
}

TEST_F(GCListTest, PrependSingleObject)
{
    GCList list;
    list.prepend(objects[0]);

    EXPECT_EQ(list.count(), 1);
    EXPECT_EQ(list.head(), objects[0]);
    EXPECT_EQ(list.tail(), objects[0]);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, PrependMultipleObjects)
{
    GCList list;

    for (int i = 4; i >= 0; --i)
    {
        list.prepend(objects[i]);
    }

    EXPECT_EQ(list.count(), 5);
    EXPECT_EQ(list.head(), objects[0]);
    EXPECT_EQ(list.tail(), objects[4]);
    EXPECT_TRUE(list.validate());

    EXPECT_EQ(objects[0]->next, objects[1]);
    EXPECT_EQ(objects[1]->next, objects[2]);
    EXPECT_EQ(objects[2]->next, objects[3]);
    EXPECT_EQ(objects[3]->next, objects[4]);
}

TEST_F(GCListTest, MixedAppendPrepend)
{
    GCList list;

    list.append(objects[2]);
    list.prepend(objects[1]);
    list.append(objects[3]);
    list.prepend(objects[0]);
    list.append(objects[4]);

    EXPECT_EQ(list.count(), 5);
    EXPECT_EQ(list.head(), objects[0]);
    EXPECT_EQ(list.tail(), objects[4]);
    EXPECT_TRUE(list.validate());

    EXPECT_EQ(objects[0]->next, objects[1]);
    EXPECT_EQ(objects[1]->next, objects[2]);
    EXPECT_EQ(objects[2]->next, objects[3]);
    EXPECT_EQ(objects[3]->next, objects[4]);
}

TEST_F(GCListTest, ContainsCheck)
{
    GCList list;

    list.append(objects[0]);
    list.append(objects[2]);
    list.append(objects[4]);

    EXPECT_TRUE(list.contains(objects[0]));
    EXPECT_FALSE(list.contains(objects[1]));
    EXPECT_TRUE(list.contains(objects[2]));
    EXPECT_FALSE(list.contains(objects[3]));
    EXPECT_TRUE(list.contains(objects[4]));
}

TEST_F(GCListTest, RemoveNonExistentObject)
{
    GCList list;

    list.append(objects[0]);
    list.append(objects[1]);

    list.remove(objects[5]);

    EXPECT_EQ(list.count(), 2);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, RemoveNullObject)
{
    GCList list;

    list.append(objects[0]);
    list.remove(nullptr);

    EXPECT_EQ(list.count(), 1);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, AppendNullObject)
{
    GCList list;

    list.append(nullptr);

    EXPECT_TRUE(list.empty());
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, Clear)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    list.clear();

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.count(), 0);
    EXPECT_EQ(list.head(), nullptr);
    EXPECT_EQ(list.tail(), nullptr);
    EXPECT_TRUE(list.validate());
}

TEST_F(GCListTest, IterateForward)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    int expected_id = 0;
    for (GCObject* obj = list.head(); obj != nullptr; obj = obj->next)
    {
        auto* mock = static_cast<MockGCObject*>(obj);
        EXPECT_EQ(mock->id, expected_id++);
    }
    EXPECT_EQ(expected_id, 5);
}

TEST_F(GCListTest, IterateBackward)
{
    GCList list;

    for (int i = 0; i < 5; ++i)
    {
        list.append(objects[i]);
    }

    int expected_id = 4;
    for (GCObject* obj = list.tail(); obj != nullptr; obj = obj->prev)
    {
        auto* mock = static_cast<MockGCObject*>(obj);
        EXPECT_EQ(mock->id, expected_id--);
    }
    EXPECT_EQ(expected_id, -1);
}

TEST_F(GCListTest, StressTestManyOperations)
{
    GCList list;

    for (int i = 0; i < 10; ++i)
    {
        list.append(objects[i]);
    }
    EXPECT_EQ(list.count(), 10);
    EXPECT_TRUE(list.validate());

    list.remove(objects[3]);
    list.remove(objects[7]);
    list.remove(objects[0]);
    list.remove(objects[9]);
    EXPECT_EQ(list.count(), 6);
    EXPECT_TRUE(list.validate());

    EXPECT_FALSE(list.contains(objects[0]));
    EXPECT_FALSE(list.contains(objects[3]));
    EXPECT_FALSE(list.contains(objects[7]));
    EXPECT_FALSE(list.contains(objects[9]));

    EXPECT_TRUE(list.contains(objects[1]));
    EXPECT_TRUE(list.contains(objects[2]));
    EXPECT_TRUE(list.contains(objects[4]));
    EXPECT_TRUE(list.contains(objects[5]));
    EXPECT_TRUE(list.contains(objects[6]));
    EXPECT_TRUE(list.contains(objects[8]));

    EXPECT_EQ(list.head(), objects[1]);

    EXPECT_EQ(list.tail(), objects[8]);
}

TEST_F(GCListTest, ConstructAfterAddingToList)
{
    GCList list;

    void* raw_mem = std::malloc(sizeof(MockGCObject));
    auto* obj_ptr = static_cast<MockGCObject*>(raw_mem);

    obj_ptr->next = nullptr;
    obj_ptr->prev = nullptr;

    list.append(obj_ptr);
    EXPECT_EQ(list.count(), 1);
    EXPECT_TRUE(list.validate());

    std::construct_at(obj_ptr, 42);

    EXPECT_TRUE(list.validate());
    EXPECT_EQ(list.count(), 1);
    EXPECT_EQ(list.head(), obj_ptr);
    EXPECT_EQ(list.tail(), obj_ptr);

    list.remove(obj_ptr);
    std::destroy_at(obj_ptr);
    std::free(raw_mem);
}
