#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <casket/lock_free/lf_object_pool.hpp>

using namespace casket::lf;

class LockFreeObjectPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

// Тестовый класс с подсчетом конструкторов/деструкторов
class TrackedObject
{
public:
    static std::atomic<size_t> constructed;
    static std::atomic<size_t> destructed;
    static std::atomic<size_t> default_constructed;

    int value;

    TrackedObject()
        : value(0)
    {
        constructed++;
        default_constructed++;
    }

    explicit TrackedObject(int val)
        : value(val)
    {
        constructed++;
    }

    TrackedObject(const TrackedObject& other)
        : value(other.value)
    {
        constructed++;
    }

    TrackedObject(TrackedObject&& other) noexcept
        : value(other.value)
    {
        constructed++;
        other.value = 0;
    }

    ~TrackedObject()
    {
        destructed++;
    }

    static void resetCounters()
    {
        constructed = 0;
        destructed = 0;
        default_constructed = 0;
    }
};

std::atomic<size_t> TrackedObject::constructed{0};
std::atomic<size_t> TrackedObject::destructed{0};
std::atomic<size_t> TrackedObject::default_constructed{0};

// ============================================================================
// Basic functionality tests
// ============================================================================

TEST_F(LockFreeObjectPoolTest, ConstructorInitializesCorrectly)
{
    ObjectPool<int> pool(100);

    EXPECT_EQ(pool.size(), 100);
    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), 100);
    EXPECT_FALSE(pool.empty());
}

TEST_F(LockFreeObjectPoolTest, AcquireReturnsValidPointer)
{
    ObjectPool<int> pool(100);

    int* obj = pool.acquire();
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(pool.activeCount(), 1);
    EXPECT_EQ(pool.freeCount(), 99);

    pool.release(obj);
    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), 100);
}

TEST_F(LockFreeObjectPoolTest, AcquireFailsWhenPoolIsEmpty)
{
    ObjectPool<int> pool(10);
    std::vector<int*> objects;

    // Acquire all objects
    for (int i = 0; i < 10; ++i)
    {
        int* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        objects.push_back(obj);
    }

    // Next acquire should fail
    EXPECT_EQ(pool.acquire(), nullptr);
    EXPECT_EQ(pool.activeCount(), 10);
    EXPECT_EQ(pool.freeCount(), 0);

    // Release all
    for (int* obj : objects)
    {
        pool.release(obj);
    }
    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), 10);
}

TEST_F(LockFreeObjectPoolTest, AcquireAndReleaseMultipleTimes)
{
    ObjectPool<int> pool(50);

    for (int i = 0; i < 100; ++i)
    {
        int* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        *obj = i;
        EXPECT_EQ(*obj, i);
        pool.release(obj);
    }

    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), 50);
}

TEST_F(LockFreeObjectPoolTest, TrivialTypesNoConstructorCalls)
{
    ObjectPool<int> pool(100);

    // For trivial types, constructors shouldn't be called repeatedly
    int* obj1 = pool.acquire();
    *obj1 = 42;
    pool.release(obj1);

    int* obj2 = pool.acquire();
    // Value may be unchanged for trivial types
    pool.release(obj2);
}

// ============================================================================
// Thread safety tests
// ============================================================================

TEST_F(LockFreeObjectPoolTest, ConcurrentAcquireAndRelease)
{
    constexpr int numThreads = 16;
    constexpr int operationsPerThread = 1000;
    constexpr int poolSize = 256;

    ObjectPool<int> pool(poolSize);
    std::atomic<size_t> successfulAcquires{0};
    std::atomic<size_t> failedAcquires{0};

    auto worker = [&]()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1, 10);

        for (int i = 0; i < operationsPerThread; ++i)
        {
            int* obj = pool.acquire();
            if (obj)
            {
                *obj = i;
                successfulAcquires++;

                // Simulate some work
                std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));

                pool.release(obj);
            }
            else
            {
                failedAcquires++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Should have many successful acquires
    EXPECT_GT(successfulAcquires.load(), 0);
    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), poolSize);
}

TEST_F(LockFreeObjectPoolTest, StressTestWithRealObjects)
{
    constexpr int numThreads = 24;
    constexpr int poolSize = 128;
    constexpr int operationsPerThread = 10000;

    struct ComplexObject
    {
        std::vector<int> data;
        std::string name;
        int id;

        ComplexObject()
            : data(100, 0)
            , name("test")
            , id(0)
        {
        }

        void reset(int newId)
        {
            id = newId;
            for (auto& val : data)
            {
                val = newId;
            }
            name = std::to_string(newId);
        }
    };

    ObjectPool<ComplexObject> pool(poolSize);
    std::atomic<size_t> successCount{0};

    auto worker = [&]()
    {
        for (int i = 0; i < operationsPerThread; ++i)
        {
            ComplexObject* obj = pool.acquire();
            if (obj)
            {
                obj->reset(i);
                EXPECT_EQ(obj->id, i);
                EXPECT_EQ(obj->name, std::to_string(i));
                for (auto val : obj->data)
                {
                    EXPECT_EQ(val, i);
                }
                successCount++;
                pool.release(obj);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_GT(successCount.load(), 0);
    EXPECT_EQ(pool.activeCount(), 0);
}

// ============================================================================
// Performance tests
// ============================================================================

TEST_F(LockFreeObjectPoolTest, PoolPerformanceVsNewDelete)
{
    constexpr int iterations = 1000000;

    // Test with new/delete
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        int* obj = new int(i);
        delete obj;
    }
    auto newDeleteTime = std::chrono::high_resolution_clock::now() - start;

    // Test with pool
    ObjectPool<int> pool(1024);
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        int* obj = pool.acquire();
        if (obj)
        {
            *obj = i;
            pool.release(obj);
        }
    }
    auto poolTime = std::chrono::high_resolution_clock::now() - start;

    // Pool should be faster (or at least not significantly slower)
    // Note: In debug builds this might not hold
    std::cout << "new/delete time: " << newDeleteTime.count() << " ns\n";
    std::cout << "pool time: " << poolTime.count() << " ns\n";

    // We don't enforce this in test, just for observation
    SUCCEED();
}

// ============================================================================
// Edge cases tests
// ============================================================================

TEST_F(LockFreeObjectPoolTest, ZeroSizePool)
{
    EXPECT_THROW(ObjectPool<int> pool(0), std::invalid_argument);
}

TEST_F(LockFreeObjectPoolTest, ReleaseNullptrDoesNothing)
{
    ObjectPool<int> pool(10);
    size_t beforeActive = pool.activeCount();
    size_t beforeFree = pool.freeCount();

    pool.release(nullptr);

    EXPECT_EQ(pool.activeCount(), beforeActive);
    EXPECT_EQ(pool.freeCount(), beforeFree);
}

TEST_F(LockFreeObjectPoolTest, SingleObjectReusedCorrectly)
{
    ObjectPool<int> pool(1);

    int* obj1 = pool.acquire();
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(pool.acquire(), nullptr);

    pool.release(obj1);

    int* obj2 = pool.acquire();
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj1, obj2); // Same address reused

    pool.release(obj2);
    EXPECT_EQ(pool.activeCount(), 0);
    EXPECT_EQ(pool.freeCount(), 1);
}

TEST_F(LockFreeObjectPoolTest, LargePoolAllocation)
{
    constexpr size_t largeSize = 100000;
    ObjectPool<int> pool(largeSize);

    EXPECT_EQ(pool.size(), largeSize);
    EXPECT_EQ(pool.freeCount(), largeSize);

    std::vector<int*> objects;
    for (size_t i = 0; i < largeSize; ++i)
    {
        int* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        objects.push_back(obj);
    }

    EXPECT_EQ(pool.acquire(), nullptr);

    for (int* obj : objects)
    {
        pool.release(obj);
    }

    EXPECT_EQ(pool.freeCount(), largeSize);
}
