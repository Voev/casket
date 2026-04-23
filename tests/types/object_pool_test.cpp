#include <gtest/gtest.h>
#include <casket/types/object_pool.hpp>

using namespace casket;

struct TestObject
{
    int value;
    char data[64];

    TestObject()
        : value(0)
    {
    }
    explicit TestObject(int v)
        : value(v)
    {
    }
    TestObject(int v, char c)
        : value(v)
    {
        data[0] = c;
    }

    void reset()
    {
        value = 0;
    }
};

TEST(ObjectPoolTest, AcquireReleaseFromPool)
{
    ObjectPool<TestObject> pool(10);

    TestObject* obj1 = pool.acquire();
    ASSERT_NE(obj1, nullptr);
    obj1->value = 42;

    TestObject* obj2 = pool.acquire();
    ASSERT_NE(obj2, nullptr);
    obj2->value = 100;

    EXPECT_EQ(pool.poolSize(), 10);

    pool.release(obj1);
    pool.release(obj2);
}

TEST(ObjectPoolTest, AcquireWithArgs)
{
    ObjectPool<TestObject> pool(10);

    TestObject* obj = pool.acquire(42);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);

    pool.release(obj);
}

TEST(ObjectPoolTest, AcquireWithMultipleArgs)
{
    ObjectPool<TestObject> pool(10);

    TestObject* obj = pool.acquire(42, 'X');
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);
    EXPECT_EQ(obj->data[0], 'X');

    pool.release(obj);
}

TEST(ObjectPoolTest, PoolExhaustionStrictPolicy)
{
    ObjectPool<TestObject, StrictHeapPolicy> pool(3);

    TestObject* obj1 = pool.acquire();
    TestObject* obj2 = pool.acquire();
    TestObject* obj3 = pool.acquire();

    ASSERT_NE(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    ASSERT_NE(obj3, nullptr);

    TestObject* obj4 = pool.acquire();
    EXPECT_EQ(obj4, nullptr);

    pool.release(obj1);
    pool.release(obj2);
    pool.release(obj3);
}

TEST(ObjectPoolTest, PoolExhaustionFallbackPolicy)
{
    ObjectPool<TestObject, HeapAllocationPolicy> pool(3);

    TestObject* obj1 = pool.acquire();
    TestObject* obj2 = pool.acquire();
    TestObject* obj3 = pool.acquire();
    TestObject* obj4 = pool.acquire();

    ASSERT_NE(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    ASSERT_NE(obj3, nullptr);
    ASSERT_NE(obj4, nullptr);

    pool.release(obj1);
    pool.release(obj2);
    pool.release(obj3);
    pool.release(obj4);
}

TEST(ObjectPoolTest, ReuseAfterRelease)
{
    ObjectPool<TestObject> pool(2);

    TestObject* obj1 = pool.acquire(10);
    TestObject* obj2 = pool.acquire(20);

    EXPECT_EQ(obj1->value, 10);
    EXPECT_EQ(obj2->value, 20);

    pool.release(obj1);
    pool.release(obj2);

    TestObject* obj3 = pool.acquire(30);
    TestObject* obj4 = pool.acquire(40);

    EXPECT_EQ(obj3->value, 30);
    EXPECT_EQ(obj4->value, 40);
}

TEST(ObjectPoolTest, ZeroSizePool)
{
    ObjectPool<TestObject, StrictHeapPolicy> pool(0);

    TestObject* obj = pool.acquire();
    EXPECT_EQ(obj, nullptr);
}

TEST(ObjectPoolTest, LargePool)
{
    const size_t poolSize = 10000;
    ObjectPool<TestObject> pool(poolSize);

    std::vector<TestObject*> objects;
    for (size_t i = 0; i < poolSize; ++i)
    {
        TestObject* obj = pool.acquire(static_cast<int>(i));
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(obj->value, static_cast<int>(i));
        objects.push_back(obj);
    }

    TestObject* extra = pool.acquire();
    EXPECT_NE(extra, nullptr);

    for (auto* obj : objects)
    {
        pool.release(obj);
    }
    pool.release(extra);
}

TEST(ObjectPoolTest, ClearPool)
{
    ObjectPool<TestObject> pool(5);

    std::vector<TestObject*> objects;
    for (int i = 0; i < 5; ++i)
    {
        objects.push_back(pool.acquire(i));
    }

    for (auto* obj : objects)
    {
        pool.release(obj);
    }

    pool.clear();

    TestObject* obj = pool.acquire();
    ASSERT_NE(obj, nullptr);
    pool.release(obj);
}

TEST(ObjectPoolTest, MixedHeapAndPool)
{
    ObjectPool<TestObject, HeapAllocationPolicy> pool(2);

    TestObject* poolObj1 = pool.acquire(10);
    TestObject* poolObj2 = pool.acquire(20);
    TestObject* heapObj = pool.acquire(30);

    EXPECT_NE(poolObj1, nullptr);
    EXPECT_NE(poolObj2, nullptr);
    EXPECT_NE(heapObj, nullptr);

    pool.release(poolObj1);
    pool.release(poolObj2);
    pool.release(heapObj);
}

TEST(ObjectPoolTest, ReleaseNullptr)
{
    ObjectPool<TestObject> pool(1);

    pool.release(nullptr);

    TestObject* obj = pool.acquire();
    ASSERT_NE(obj, nullptr);
}

TEST(ObjectPoolTest, AcquireAfterClear)
{
    ObjectPool<TestObject> pool(3);

    TestObject* obj1 = pool.acquire(1);
    TestObject* obj2 = pool.acquire(2);

    pool.release(obj1);
    pool.release(obj2);

    pool.clear();

    TestObject* obj3 = pool.acquire(3);
    ASSERT_NE(obj3, nullptr);
    EXPECT_EQ(obj3->value, 3);

    pool.release(obj3);
}

TEST(ObjectPoolTest, MultipleAcquireWithDifferentArgs)
{
    ObjectPool<TestObject> pool(5);

    TestObject* obj1 = pool.acquire(10);
    TestObject* obj2 = pool.acquire(20);
    TestObject* obj3 = pool.acquire(30);

    EXPECT_EQ(obj1->value, 10);
    EXPECT_EQ(obj2->value, 20);
    EXPECT_EQ(obj3->value, 30);

    pool.release(obj1);
    pool.release(obj2);
    pool.release(obj3);
}

TEST(ObjectPoolTest, StringObject)
{
    struct StringObject
    {
        std::string str;
        StringObject() = default;
        explicit StringObject(const std::string& s)
            : str(s)
        {
        }
        void reset()
        {
            str.clear();
        }
    };

    ObjectPool<StringObject> pool(5);

    StringObject* obj = pool.acquire(std::string("hello"));
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->str, "hello");

    pool.release(obj);
}

TEST(ObjectPoolTest, StressTest)
{
    ObjectPool<TestObject> pool(100);
    std::vector<TestObject*> objects;

    for (int i = 0; i < 1000; ++i)
    {
        TestObject* obj = pool.acquire(i);
        if (obj)
        {
            EXPECT_EQ(obj->value, i);
            objects.push_back(obj);
        }

        if (objects.size() > 50)
        {
            pool.release(objects.front());
            objects.erase(objects.begin());
        }
    }

    for (auto* obj : objects)
    {
        pool.release(obj);
    }
}