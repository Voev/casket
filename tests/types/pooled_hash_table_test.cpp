#include <gtest/gtest.h>
#include <casket/types/pooled_hash_table.hpp>

using namespace casket;

TEST(PooledHashTableTest, PutAndGet)
{
    PooledHashTable<int, std::string> table(10);

    auto* val = table.put(1, "one");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");

    val = table.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");

    val = table.get(2);
    EXPECT_EQ(val, nullptr);
}

TEST(PooledHashTableTest, PutOverwritesExisting)
{
    PooledHashTable<int, std::string> table(10);

    table.put(1, "one");
    auto* val = table.get(1);
    EXPECT_EQ(*val, "one");

    table.put(1, "ONE");
    val = table.get(1);
    EXPECT_EQ(*val, "ONE");

    EXPECT_EQ(table.size(), 1);
}

TEST(PooledHashTableTest, Remove)
{
    PooledHashTable<int, std::string> table(10);

    table.put(1, "one");
    table.put(2, "two");

    EXPECT_TRUE(table.remove(1));
    EXPECT_EQ(table.get(1), nullptr);
    EXPECT_EQ(table.size(), 1);

    EXPECT_FALSE(table.remove(999));
    EXPECT_EQ(table.size(), 1);
}

TEST(PooledHashTableTest, Contains)
{
    PooledHashTable<int, std::string> table(10);

    table.put(1, "one");

    EXPECT_TRUE(table.contains(1));
    EXPECT_FALSE(table.contains(2));
}

TEST(PooledHashTableTest, Clear)
{
    PooledHashTable<int, std::string> table(10);

    table.put(1, "one");
    table.put(2, "two");
    table.put(3, "three");

    EXPECT_EQ(table.size(), 3);

    table.clear();

    EXPECT_EQ(table.size(), 0);
    EXPECT_EQ(table.get(1), nullptr);
    EXPECT_EQ(table.get(2), nullptr);
    EXPECT_EQ(table.get(3), nullptr);
}

TEST(PooledHashTableTest, PoolExhaustion)
{
    PooledHashTable<int, std::string> table(3);

    table.put(1, "one");
    table.put(2, "two");
    table.put(3, "three");
    auto* val = table.put(4, "four");

    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(table.size(), 3);
    EXPECT_NE(table.get(1), nullptr);
    EXPECT_NE(table.get(2), nullptr);
    EXPECT_NE(table.get(3), nullptr);
    EXPECT_EQ(table.get(4), nullptr);
}

TEST(PooledHashTableTest, StringKeys)
{
    PooledHashTable<std::string, int> table(10);

    table.put("one", 1);
    table.put("two", 2);
    table.put("three", 3);

    auto* val = table.get("two");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 2);

    EXPECT_NE(table.get("one"), nullptr);
    EXPECT_NE(table.get("three"), nullptr);
    EXPECT_EQ(table.size(), 3);
}

TEST(PooledHashTableTest, ComplexObject)
{
    struct Complex
    {
        int a;
        double b;
        std::string c;
    };

    PooledHashTable<int, Complex> table(10);

    Complex obj{42, 3.14, "test"};
    table.put(1, obj);

    auto* retrieved = table.get(1);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->a, 42);
    EXPECT_DOUBLE_EQ(retrieved->b, 3.14);
    EXPECT_EQ(retrieved->c, "test");
}

TEST(PooledHashTableTest, ManyOperations)
{
    PooledHashTable<int, int> table(100);

    for (int i = 0; i < 100; ++i)
    {
        table.put(i, i * i);
    }

    EXPECT_EQ(table.size(), 100);

    for (int i = 0; i < 100; ++i)
    {
        auto* val = table.get(i);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i * i);
    }
}

TEST(PooledHashTableTest, ZeroSizePool)
{
    PooledHashTable<int, int> table(0);

    auto* val = table.put(1, 100);
    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(table.size(), 0);
}

TEST(PooledHashTableTest, CollisionHandling)
{
    PooledHashTable<int, int> table(10);

    for (int i = 0; i < 100; ++i)
    {
        table.put(i, i);
    }

    EXPECT_EQ(table.size(), 10);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_NE(table.get(i), nullptr);
    }

    for (int i = 10; i < 100; ++i)
    {
        EXPECT_EQ(table.get(i), nullptr);
    }
}

TEST(PooledHashTableTest, UpdateAfterRemove)
{
    PooledHashTable<int, std::string> table(10);

    table.put(1, "one");
    EXPECT_EQ(*table.get(1), "one");

    table.remove(1);
    EXPECT_EQ(table.get(1), nullptr);

    auto* val = table.put(1, "ONE");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "ONE");
    EXPECT_EQ(table.size(), 1);
}

TEST(PooledHashTableTest, PointerValues)
{
    PooledHashTable<int, int*> table(10);

    int a = 10, b = 20, c = 30;

    table.put(1, &a);
    table.put(2, &b);
    table.put(3, &c);

    auto* val = table.get(2);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, &b);
    EXPECT_EQ(**val, 20);
}

TEST(PooledHashTableTest, RepeatedPutSameKey)
{
    PooledHashTable<int, int> table(10);

    for (int i = 0; i < 100; ++i)
    {
        table.put(1, i);
    }

    auto* val = table.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 99);
    EXPECT_EQ(table.size(), 1);
}

TEST(PooledHashTableTest, DoubleKeys)
{
    PooledHashTable<double, std::string> table(10);

    table.put(3.14, "pi");
    table.put(2.718, "e");

    auto* val = table.get(3.14);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "pi");

    EXPECT_EQ(table.size(), 2);
}

TEST(PooledHashTableTest, StressTest)
{
    PooledHashTable<int, int> table(1000);

    for (int i = 0; i < 100000; ++i)
    {
        table.put(i % 500, i);
    }

    EXPECT_EQ(table.size(), 500);

    for (int i = 0; i < 500; ++i)
    {
        auto* val = table.get(i);
        ASSERT_NE(val, nullptr);
    }
}

TEST(PooledHashTableTest, LargePoolSize)
{
    PooledHashTable<int, int> table(10000);

    for (int i = 0; i < 5000; ++i)
    {
        table.put(i, i);
    }

    EXPECT_EQ(table.size(), 5000);

    for (int i = 0; i < 5000; ++i)
    {
        auto* val = table.get(i);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);
    }
}

TEST(PooledHashTableTest, ClearAfterFull)
{
    PooledHashTable<int, int> table(5);

    for (int i = 0; i < 5; ++i)
    {
        table.put(i, i);
    }

    EXPECT_EQ(table.size(), 5);

    table.clear();

    EXPECT_EQ(table.size(), 0);

    for (int i = 0; i < 5; ++i)
    {
        auto* val = table.put(i, i * 10);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i * 10);
    }
}