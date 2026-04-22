#include <gtest/gtest.h>
#include <casket/types/lru_cache.hpp>

using namespace casket;

TEST(LruCacheTest, PutAndGet)
{
    LruCache<int, std::string> cache(10);

    auto* val = cache.put(1, "one");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");

    val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");

    val = cache.get(2);
    EXPECT_EQ(val, nullptr);
}

TEST(LruCacheTest, PutOverwritesExisting)
{
    LruCache<int, std::string> cache(10);

    cache.put(1, "one");
    auto* val = cache.get(1);
    EXPECT_EQ(*val, "one");

    cache.put(1, "ONE");
    val = cache.get(1);
    EXPECT_EQ(*val, "ONE");

    EXPECT_EQ(cache.size(), 1);
}

TEST(LruCacheTest, Remove)
{
    LruCache<int, std::string> cache(10);

    cache.put(1, "one");
    cache.put(2, "two");

    EXPECT_TRUE(cache.remove(1));
    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_EQ(cache.size(), 1);

    EXPECT_FALSE(cache.remove(999));
    EXPECT_EQ(cache.size(), 1);
}

TEST(LruCacheTest, Contains)
{
    LruCache<int, std::string> cache(10);

    cache.put(1, "one");

    EXPECT_TRUE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
}

TEST(LruCacheTest, Release)
{
    LruCache<int, std::string> cache(10);

    cache.put(1, "one");
    cache.release(1);

    auto* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");
}

TEST(LruCacheTest, Clear)
{
    LruCache<int, std::string> cache(10);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3);

    cache.clear();

    EXPECT_EQ(cache.size(), 0);
    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_EQ(cache.get(2), nullptr);
    EXPECT_EQ(cache.get(3), nullptr);
}

TEST(LruCacheTest, EvictLRUWhenFull)
{
    LruCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3);

    cache.get(3);
    cache.put(4, "four");

    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_NE(cache.get(2), nullptr);
    EXPECT_NE(cache.get(3), nullptr);
    EXPECT_NE(cache.get(4), nullptr);
    EXPECT_EQ(cache.size(), 3);
}

TEST(LruCacheTest, ReleaseUpdatesLRU)
{
    LruCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    // Order: 1 (older), 2, 3 (newer)

    cache.release(1);

    // Order: 2 (older), 3, 1 (newer)

    cache.put(4, "four");

    EXPECT_NE(cache.get(1), nullptr);
    EXPECT_EQ(cache.get(2), nullptr);
    EXPECT_NE(cache.get(3), nullptr);
    EXPECT_NE(cache.get(4), nullptr);
}

TEST(LruCacheTest, PoolExhaustion)
{
    LruCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    cache.put(4, "four");

    EXPECT_EQ(cache.size(), 3);
    EXPECT_EQ(cache.get(1), nullptr);
}

TEST(LruCacheTest, ComplexObject)
{
    struct Complex
    {
        int a;
        double b;
        std::string c;
    };

    LruCache<int, Complex> cache(10);

    Complex obj{42, 3.14, "test"};
    cache.put(1, obj);

    auto* retrieved = cache.get(1);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->a, 42);
    EXPECT_DOUBLE_EQ(retrieved->b, 3.14);
    EXPECT_EQ(retrieved->c, "test");
}

TEST(LruCacheTest, ManyOperations)
{
    LruCache<int, int> cache(100);

    for (int i = 0; i < 1000; ++i)
    {
        cache.put(i, i * i);
    }

    EXPECT_EQ(cache.size(), 100);

    for (int i = 0; i < 100; ++i)
    {
        auto* val = cache.get(i + 900);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, (i + 900) * (i + 900));
    }

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(cache.get(i), nullptr);
    }
}

TEST(LruCacheTest, StressTest)
{
    LruCache<int, int> cache(1000);

    for (int i = 0; i < 100000; ++i)
    {
        cache.put(i, i);
        if (i % 2 == 0)
        {
            cache.get(i / 2);
        }
    }

    EXPECT_EQ(cache.size(), 1000);
}

TEST(LruCacheTest, StringKeys)
{
    LruCache<std::string, int> cache(3);

    cache.put("one", 1);
    cache.put("two", 2);
    cache.put("three", 3);

    auto* val = cache.get("two");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 2);

    cache.put("four", 4);
    cache.put("five", 5);

    EXPECT_EQ(cache.get("one"), nullptr);
    EXPECT_NE(cache.get("two"), nullptr);
    EXPECT_EQ(cache.get("three"), nullptr);
    EXPECT_NE(cache.get("four"), nullptr);
    EXPECT_NE(cache.get("five"), nullptr);
}

TEST(LruCacheTest, PointerValues)
{
    LruCache<int, int*> cache(10);

    int a = 10, b = 20, c = 30;

    cache.put(1, &a);
    cache.put(2, &b);
    cache.put(3, &c);

    auto* val = cache.get(2);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, &b);
    EXPECT_EQ(**val, 20);
}

TEST(LruCacheTest, ZeroSizePool)
{
    LruCache<int, int> cache(0);

    auto* val = cache.put(1, 100);
    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(cache.size(), 0);
}

TEST(LruCacheTest, UpdateAfterEviction)
{
    LruCache<int, int> cache(2);

    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_NE(cache.get(2), nullptr);
    EXPECT_NE(cache.get(3), nullptr);

    cache.put(1, 100);

    EXPECT_NE(cache.get(1), nullptr);
    EXPECT_EQ(cache.size(), 2);
}

TEST(LruCacheTest, RemoveNonExistent)
{
    LruCache<int, int> cache(10);

    EXPECT_FALSE(cache.remove(999));
    EXPECT_EQ(cache.size(), 0);
}

TEST(LruCacheTest, RepeatedPutSameKey)
{
    LruCache<int, int> cache(10);

    for (int i = 0; i < 100; ++i)
    {
        cache.put(1, i);
    }

    auto* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 99);
    EXPECT_EQ(cache.size(), 1);
}

TEST(LruCacheTest, MixedTypes)
{
    LruCache<double, std::string> cache(10);

    cache.put(3.14, "pi");
    cache.put(2.718, "e");

    auto* val = cache.get(3.14);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "pi");

    EXPECT_EQ(cache.size(), 2);
}