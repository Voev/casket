#include <gtest/gtest.h>
#include <casket/types/object_pool.hpp>

using namespace casket;

class TestObject
{
public:
    static int constructorCount;
    static int destructorCount;
    static int copyCount;
    static int moveCount;
    static int paramConstructorCount;

    int value;
    std::string name;

    TestObject()
        : value(0)
        , name("default")
    {
        constructorCount++;
    }

    explicit TestObject(int v)
        : value(v)
        , name("param")
    {
        paramConstructorCount++;
    }

    TestObject(int v, std::string n)
        : value(v)
        , name(std::move(n))
    {
        paramConstructorCount++;
    }

    TestObject(const TestObject& other)
        : value(other.value)
        , name(other.name)
    {
        copyCount++;
    }

    TestObject(TestObject&& other) noexcept
        : value(other.value)
        , name(std::move(other.name))
    {
        moveCount++;
        other.value = -1;
    }

    ~TestObject()
    {
        destructorCount++;
    }

    TestObject& operator=(const TestObject& other)
    {
        value = other.value;
        name = other.name;
        copyCount++;
        return *this;
    }

    TestObject& operator=(TestObject&& other) noexcept
    {
        value = other.value;
        name = std::move(other.name);
        other.value = -1;
        moveCount++;
        return *this;
    }

    static void resetCounters()
    {
        constructorCount = 0;
        destructorCount = 0;
        copyCount = 0;
        moveCount = 0;
        paramConstructorCount = 0;
    }
};

int TestObject::constructorCount = 0;
int TestObject::destructorCount = 0;
int TestObject::copyCount = 0;
int TestObject::moveCount = 0;
int TestObject::paramConstructorCount = 0;

class ObjectPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        TestObject::resetCounters();
    }

    void TearDown() override
    {
        TestObject::resetCounters();
    }
};

TEST_F(ObjectPoolTest, BasicCreateDestroy)
{
    ObjectPool<TestObject> pool;

    ASSERT_EQ(pool.size(), 0);
    ASSERT_EQ(pool.capacity(), 0);
    ASSERT_TRUE(pool.empty());

    TestObject* obj = pool.create(42, "test");
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->value, 42);
    ASSERT_EQ(obj->name, "test");
    ASSERT_EQ(pool.size(), 1);
    ASSERT_FALSE(pool.empty());

    pool.destroy(obj);
    ASSERT_EQ(pool.size(), 0);
    ASSERT_TRUE(pool.empty());
}

TEST_F(ObjectPoolTest, ConstructorDestructorCount)
{
    {
        ObjectPool<TestObject> pool;

        ASSERT_EQ(TestObject::constructorCount, 0);
        ASSERT_EQ(TestObject::paramConstructorCount, 0);
        ASSERT_EQ(TestObject::destructorCount, 0);

        TestObject* obj1 = pool.create(1, "first");
        ASSERT_EQ(TestObject::paramConstructorCount, 1);
        ASSERT_EQ(TestObject::constructorCount, 0);
        ASSERT_EQ(TestObject::destructorCount, 0);

        TestObject* obj2 = pool.create(2, "second");
        ASSERT_EQ(TestObject::paramConstructorCount, 2);
        ASSERT_EQ(TestObject::constructorCount, 0);

        pool.destroy(obj1);
        ASSERT_EQ(TestObject::destructorCount, 1);

        pool.destroy(obj2);
        ASSERT_EQ(TestObject::destructorCount, 2);
    }
    ASSERT_EQ(TestObject::destructorCount, 2);
}

TEST_F(ObjectPoolTest, MoveSemanticsNoExtraConstructions)
{
    ObjectPool<TestObject> pool1(5);

    for (int i = 0; i < 3; ++i)
    {
        pool1.create(i, "obj" + std::to_string(i));
    }

    size_t originalSize = pool1.size();
    size_t originalCapacity = pool1.capacity();
    int originalConstructorCount = TestObject::paramConstructorCount;

    ObjectPool<TestObject> pool2 = std::move(pool1);

    ASSERT_EQ(pool2.size(), originalSize);
    ASSERT_EQ(pool2.capacity(), originalCapacity);
    ASSERT_EQ(pool1.size(), 0);
    ASSERT_EQ(pool1.capacity(), 0);
    ASSERT_TRUE(pool1.empty());

    ASSERT_EQ(TestObject::paramConstructorCount, originalConstructorCount);
}

TEST_F(ObjectPoolTest, ClearFunctionDestroysOnlyConstructedObjects)
{
    ObjectPool<TestObject> pool(10);

    std::vector<TestObject*> objects;
    for (int i = 0; i < 5; ++i)
    {
        objects.push_back(pool.create(i, "obj" + std::to_string(i)));
    }

    ASSERT_EQ(pool.size(), 5);
    ASSERT_EQ(TestObject::paramConstructorCount, 5);
    ASSERT_EQ(TestObject::destructorCount, 0);

    pool.clear();

    ASSERT_EQ(pool.size(), 0);
    ASSERT_EQ(pool.capacity(), 0);
    ASSERT_EQ(TestObject::destructorCount, 5);
}

struct ThrowOnConstruction
{
    static int constructorCount;
    static int destructorCount;

    ThrowOnConstruction()
    {
        constructorCount++;
        throw std::runtime_error("Construction failed");
    }

    ~ThrowOnConstruction()
    {
        destructorCount++;
    }
};

int ThrowOnConstruction::constructorCount = 0;
int ThrowOnConstruction::destructorCount = 0;

TEST_F(ObjectPoolTest, ExceptionSafetyNoLeaks)
{
    ObjectPool<ThrowOnConstruction> pool;

    ASSERT_THROW(pool.create(), std::runtime_error);
    ASSERT_EQ(pool.size(), 0);

    ASSERT_EQ(ThrowOnConstruction::constructorCount, 1);
    ASSERT_EQ(ThrowOnConstruction::destructorCount, 0);
}

TEST_F(ObjectPoolTest, SmartPointerHelpersProperDestruction)
{
    ObjectPool<TestObject> pool;

    int initialDestructorCount = TestObject::destructorCount;
    int initialConstructorCount = TestObject::paramConstructorCount;

    {
        auto sharedObj = make_shared_from_pool(pool, 100, "shared");
        ASSERT_EQ(sharedObj->value, 100);
        ASSERT_EQ(sharedObj->name, "shared");
        ASSERT_EQ(pool.size(), 1);
        ASSERT_EQ(TestObject::paramConstructorCount, initialConstructorCount + 1);
    }

    ASSERT_EQ(pool.size(), 0);
    ASSERT_EQ(TestObject::destructorCount, initialDestructorCount + 1);
}

TEST_F(ObjectPoolTest, ExceptionSafety)
{
    struct ThrowOnConstruction
    {
        ThrowOnConstruction()
        {
            throw std::runtime_error("Construction failed");
        }
    };

    ObjectPool<ThrowOnConstruction> pool;

    ASSERT_THROW(pool.create(), std::runtime_error);
    ASSERT_EQ(pool.size(), 0);
}

template <typename T>
class TestAllocator
{
public:
    using value_type = T;

    TestAllocator() = default;

    template <typename U>
    TestAllocator(const TestAllocator<U>&)
    {
    }

    T* allocate(std::size_t n)
    {
        allocationCount += n;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n)
    {
        deallocationCount += n;
        ::operator delete(p);
    }

    static std::size_t allocationCount;
    static std::size_t deallocationCount;

    static void resetCounters()
    {
        allocationCount = 0;
        deallocationCount = 0;
    }
};

template <typename T>
std::size_t TestAllocator<T>::allocationCount = 0;

template <typename T>
std::size_t TestAllocator<T>::deallocationCount = 0;

TEST_F(ObjectPoolTest, CustomAllocator)
{
    TestAllocator<TestObject>::resetCounters();

    ObjectPool<TestObject, TestAllocator<TestObject>> pool;
    pool.reserve(5);

    ASSERT_GT(TestAllocator<TestObject>::allocationCount, 0);

    auto obj = pool.create(42, "test");
    pool.destroy(obj);

    pool.clear();
    ASSERT_GT(TestAllocator<TestObject>::deallocationCount, 0);
}

TEST_F(ObjectPoolTest, SmartPointerHelpers)
{
    ObjectPool<TestObject> pool;

    {
        auto sharedObj = make_shared_from_pool(pool, 100, "shared");
        ASSERT_EQ(sharedObj->value, 100);
        ASSERT_EQ(sharedObj->name, "shared");
        ASSERT_EQ(pool.size(), 1);
    }
    ASSERT_EQ(pool.size(), 0);

    {
        auto uniqueObj = make_unique_from_pool(pool, 200, "unique");
        ASSERT_EQ(uniqueObj->value, 200);
        ASSERT_EQ(uniqueObj->name, "unique");
        ASSERT_EQ(pool.size(), 1);
    }
    ASSERT_EQ(pool.size(), 0);
}

TEST_F(ObjectPoolTest, InvalidDestroyThrows)
{
    ObjectPool<TestObject> pool;
    TestObject localObj;

    ASSERT_THROW(pool.destroy(&localObj), std::invalid_argument);

    TestObject* validObj = pool.create(1, "valid");
    pool.destroy(validObj);

    ASSERT_THROW(pool.destroy(validObj), std::invalid_argument);
}

TEST_F(ObjectPoolTest, LargeNumberOfObjects)
{
    const int COUNT = 1000;
    ObjectPool<TestObject> pool;

    std::vector<TestObject*> objects;
    for (int i = 0; i < COUNT; ++i)
    {
        objects.push_back(pool.create(i, "obj" + std::to_string(i)));
    }

    ASSERT_EQ(pool.size(), COUNT);

    for (int i = COUNT - 1; i >= 0; i -= 2)
    {
        pool.destroy(objects[i]);
    }

    ASSERT_EQ(pool.size(), COUNT / 2);

    for (int i = 0; i < COUNT / 2; ++i)
    {
        pool.create(i + COUNT, "newObj" + std::to_string(i));
    }

    ASSERT_EQ(pool.size(), COUNT);
}
