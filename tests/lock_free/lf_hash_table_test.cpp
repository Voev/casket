#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <atomic>
#include <casket/lock_free/lf_hash_table.hpp>

using namespace casket::lf;

TEST(LockFreeHashTableTest, BasicPutAndGet)
{
    HashTable<int, std::string, 1024> table(100);

    auto* value = table.put(1, "one");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "one");

    value = table.get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "one");

    EXPECT_EQ(table.size(), 1);
}

TEST(LockFreeHashTableTest, PutExistingKeyReturnsNullptr)
{
    HashTable<int, std::string, 1024> table(100);

    table.put(1, "one");
    auto* value = table.put(1, "two");

    EXPECT_EQ(value, nullptr);
    EXPECT_EQ(*table.get(1), "one");
    EXPECT_EQ(table.size(), 1);
}

TEST(LockFreeHashTableTest, GetNonExistentKey)
{
    HashTable<int, std::string, 1024> table(100);

    auto* value = table.get(999);
    EXPECT_EQ(value, nullptr);
}

TEST(LockFreeHashTableTest, RemoveExistingKey)
{
    HashTable<int, std::string, 1024> table(100);

    table.put(1, "one");
    EXPECT_TRUE(table.remove(1));
    EXPECT_EQ(table.get(1), nullptr);
    EXPECT_EQ(table.size(), 0);
}

TEST(LockFreeHashTableTest, RemoveNonExistentKey)
{
    HashTable<int, std::string, 1024> table(100);

    EXPECT_FALSE(table.remove(999));
    EXPECT_EQ(table.size(), 0);
}

TEST(LockFreeHashTableTest, Contains)
{
    HashTable<int, std::string, 1024> table(100);

    table.put(1, "one");
    EXPECT_TRUE(table.contains(1));
    EXPECT_FALSE(table.contains(2));
}

TEST(LockFreeHashTableTest, Clear)
{
    HashTable<int, std::string, 1024> table(100);

    for (int i = 0; i < 100; ++i)
    {
        table.put(i, "value_" + std::to_string(i));
    }

    EXPECT_EQ(table.size(), 100);
    table.clear();
    EXPECT_EQ(table.size(), 0);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(table.get(i), nullptr);
    }
}

TEST(LockFreeHashTableTest, DifferentKeyTypes)
{
    HashTable<std::string, int, 1024> table(100);

    table.put("one", 1);
    table.put("two", 2);

    EXPECT_EQ(*table.get("one"), 1);
    EXPECT_EQ(*table.get("two"), 2);
    EXPECT_EQ(table.size(), 2);
}

TEST(LockFreeHashTableTest, ComplexValueType)
{
    struct ComplexType
    {
        int a;
        double b;
        std::string c;

        bool operator==(const ComplexType& other) const
        {
            return a == other.a && b == other.b && c == other.c;
        }
    };

    HashTable<int, ComplexType, 1024> table(100);

    ComplexType value{42, 3.14, "test"};
    table.put(1, value);

    auto* retrieved = table.get(1);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->a, 42);
    EXPECT_DOUBLE_EQ(retrieved->b, 3.14);
    EXPECT_EQ(retrieved->c, "test");
}

TEST(LockFreeHashTableTest, ConcurrentPut)
{
    HashTable<int, int, 2048> table(2000);
    constexpr int numThreads = 8;
    constexpr int opsPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<bool> failed{false};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < opsPerThread; ++i)
                {
                    int key = t * opsPerThread + i;
                    auto* value = table.put(key, key);
                    if (!value || *value != key)
                    {
                        failed = true;
                    }
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_FALSE(failed);
    EXPECT_EQ(table.size(), numThreads * opsPerThread);

    for (int t = 0; t < numThreads; ++t)
    {
        for (int i = 0; i < opsPerThread; ++i)
        {
            int key = t * opsPerThread + i;
            auto* value = table.get(key);
            ASSERT_NE(value, nullptr);
            EXPECT_EQ(*value, key);
        }
    }
}

TEST(LockFreeHashTableTest, ConcurrentGet)
{
    HashTable<int, int, 1024> table(200);

    for (int i = 0; i < 100; ++i)
    {
        table.put(i, i * 100);
    }

    constexpr int numThreads = 16;
    constexpr int opsPerThread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&]()
            {
                for (int i = 0; i < opsPerThread; ++i)
                {
                    int key = i % 100;
                    auto* value = table.get(key);
                    if (value && *value == key * 100)
                    {
                        successCount++;
                    }
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(successCount, numThreads * opsPerThread);
}

TEST(LockFreeHashTableTest, ConcurrentPutAndGet)
{
    HashTable<int, int, 2048> table(1000);
    constexpr int numThreads = 8;
    constexpr int opsPerThread = 200;

    std::vector<std::thread> threads;
    std::atomic<bool> failed{false};

    for (int t = 0; t < numThreads / 2; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < opsPerThread; ++i)
                {
                    int key = t * opsPerThread + i;
                    table.put(key, key);
                }
            });
    }

    for (int t = numThreads / 2; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&]()
            {
                for (int i = 0; i < opsPerThread * 2; ++i)
                {
                    int key = i % (numThreads / 2 * opsPerThread);
                    table.get(key);
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_FALSE(failed);
}

TEST(LockFreeHashTableTest, ConcurrentRemove)
{
    HashTable<int, int, 1024> table(200);

    for (int i = 0; i < 200; ++i)
    {
        table.put(i, i);
    }

    constexpr int numThreads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> removedCount{0};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = t; i < 200; i += numThreads)
                {
                    if (table.remove(i))
                    {
                        removedCount++;
                    }
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(removedCount, 200);
    EXPECT_EQ(table.size(), 0);
}

TEST(LockFreeHashTableTest, MixedOperations)
{
    HashTable<int, int, 2048> table(1000);
    constexpr int numThreads = 12;
    constexpr int opsPerThread = 500;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                std::mt19937 rng(t);
                std::uniform_int_distribution<int> dist(0, 999);
                std::uniform_int_distribution<int> opDist(0, 2);

                for (int i = 0; i < opsPerThread; ++i)
                {
                    int key = dist(rng);
                    int op = opDist(rng);

                    if (op == 0)
                    {
                        table.put(key, key);
                    }
                    else if (op == 1)
                    {
                        table.get(key);
                    }
                    else
                    {
                        table.remove(key);
                    }
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(errors, 0);
}

TEST(LockFreeHashTableTest, SameKeyRaceCondition)
{
    HashTable<int, int, 1024> table(100);
    constexpr int numThreads = 16;
    constexpr int attemptsPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successPuts{0};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&]()
            {
                for (int i = 0; i < attemptsPerThread; ++i)
                {
                    if (table.put(42, i) != nullptr)
                    {
                        successPuts++;
                    }
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(successPuts, 1);
    EXPECT_EQ(table.size(), 1);

    auto* value = table.get(42);
    ASSERT_NE(value, nullptr);
}

TEST(LockFreeHashTableTest, HighCollisionTest)
{
    struct BadHash
    {
        size_t operator()(int key) const
        {
            return key % 16;
        }
    };

    struct ValueType
    {
        int data;
        std::array<char, 64> padding;
    };

    HashTable<int, ValueType, 16> table(1000);
    constexpr int numThreads = 8;
    constexpr int keysPerThread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < keysPerThread; ++i)
                {
                    int key = t * keysPerThread + i;
                    table.put(key, ValueType{key, {}});
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(table.size(), numThreads * keysPerThread);

    for (int t = 0; t < numThreads; ++t)
    {
        for (int i = 0; i < keysPerThread; ++i)
        {
            int key = t * keysPerThread + i;
            auto* value = table.get(key);
            ASSERT_NE(value, nullptr);
            EXPECT_EQ(value->data, key);
        }
    }
}

TEST(LockFreeHashTableTest, EmptyTable)
{
    HashTable<int, int, 1024> table(100);

    EXPECT_EQ(table.size(), 0);
    EXPECT_EQ(table.get(1), nullptr);
    EXPECT_FALSE(table.remove(1));
    EXPECT_FALSE(table.contains(1));
    table.clear();
}

TEST(LockFreeHashTableTest, PoolExhaustion)
{
    HashTable<int, int, 1024> table(10);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_NE(table.put(i, i), nullptr);
    }

    EXPECT_EQ(table.put(10, 10), nullptr);
    EXPECT_EQ(table.size(), 10);

    table.remove(5);
    EXPECT_NE(table.put(10, 10), nullptr);
    EXPECT_EQ(table.size(), 10);
}

TEST(LockFreeHashTableTest, LargeKeysAndValues)
{
    std::string largeKey(1024, 'A');
    std::array<int, 1000> largeValue{};
    largeValue.fill(42);

    HashTable<std::string, std::array<int, 1000>, 1024> table(10);

    table.put(largeKey, largeValue);
    auto* retrieved = table.get(largeKey);

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ((*retrieved)[500], 42);
}

TEST(LockFreeHashTableTest, ForEachTest)
{
    HashTable<int, int, 1024> table(100);

    for (int i = 0; i < 50; ++i)
    {
        table.put(i, i * 2);
    }

    int sum = 0;
    int count = 0;
    table.forEach(
        [&sum, &count](int key, int& value)
        {
            sum += value;
            count++;
            EXPECT_EQ(value, key * 2);
        });

    EXPECT_EQ(count, 50);
    EXPECT_EQ(sum, 2450);
}

TEST(LockFreeHashTableTest, ConcurrentForEach)
{
    HashTable<int, int, 1024> table(100);

    for (int i = 0; i < 100; ++i)
    {
        table.put(i, i);
    }

    std::vector<std::thread> threads;
    std::atomic<int> totalIterations{0};

    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&]() { table.forEach([&](int, int&) { totalIterations++; }); });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(totalIterations, 400);
}
