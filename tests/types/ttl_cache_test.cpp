#include <gtest/gtest.h>
#include <thread>
#include <casket/types/ttl_cache.hpp>

using namespace casket;

TEST(TtlCacheTest, ConstructorCreatesEmptyCache)
{
    TtlCache<int, int> cache(100);
    EXPECT_EQ(cache.size(), 0);
    EXPECT_EQ(cache.capacity(), 100);
    EXPECT_TRUE(cache.empty());
    EXPECT_FALSE(cache.full());
}

TEST(TtlCacheTest, PutAndGetValue)
{
    TtlCache<int, std::string> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, "one", expiry);

    std::string* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");
    EXPECT_EQ(cache.size(), 1);
}

TEST(TtlCacheTest, GetReturnsNullptrForMissingKey)
{
    TtlCache<int, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, 100, expiry);

    EXPECT_EQ(cache.get(999), nullptr);
}

TEST(TtlCacheTest, UpdateExistingValue)
{
    TtlCache<int, std::string> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, "one", expiry);
    cache.put(1, "updated", expiry);

    std::string* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "updated");
    EXPECT_EQ(cache.size(), 1);
}

TEST(TtlCacheTest, ContainsReturnsCorrectStatus)
{
    TtlCache<int, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(42, 100, expiry);

    EXPECT_TRUE(cache.contains(42));
    EXPECT_FALSE(cache.contains(43));
}

TEST(TtlCacheTest, EraseRemovesElement)
{
    TtlCache<int, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, 100, expiry);
    cache.put(2, 200, expiry);

    EXPECT_EQ(cache.size(), 2);

    cache.erase(1);

    EXPECT_EQ(cache.size(), 1);
    EXPECT_FALSE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
}

TEST(TtlCacheTest, ClearRemovesAllElements)
{
    TtlCache<int, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    for (int i = 0; i < 5; ++i)
    {
        cache.put(i, i * 100, expiry);
    }

    EXPECT_EQ(cache.size(), 5);

    cache.clear();

    EXPECT_EQ(cache.size(), 0);
    EXPECT_TRUE(cache.empty());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_FALSE(cache.contains(i));
    }
}

TEST(TtlCacheTest, EvictsLeastRecentlyUsedWhenFull)
{
    TtlCache<int, std::string> cache(3);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, "one", expiry);
    cache.put(2, "two", expiry);
    cache.put(3, "three", expiry);

    EXPECT_EQ(cache.size(), 3);
    EXPECT_TRUE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
    EXPECT_TRUE(cache.contains(3));

    cache.get(1);

    cache.put(4, "four", expiry);

    EXPECT_EQ(cache.size(), 3);
    EXPECT_TRUE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
    EXPECT_TRUE(cache.contains(3));
    EXPECT_TRUE(cache.contains(4));
}

TEST(TtlCacheTest, GetUpdatesLRUOrder)
{
    TtlCache<int, std::string> cache(3);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, "one", expiry);
    cache.put(2, "two", expiry);
    cache.put(3, "three", expiry);

    cache.get(1);
    cache.get(2);

    cache.put(4, "four", expiry);

    EXPECT_FALSE(cache.contains(3));
    EXPECT_TRUE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
    EXPECT_TRUE(cache.contains(4));
}

TEST(TtlCacheTest, PutOnExistingKeyUpdatesLRU)
{
    TtlCache<int, std::string> cache(3);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, "one", expiry);
    cache.put(2, "two", expiry);
    cache.put(3, "three", expiry);

    cache.put(1, "one_updated", expiry);

    cache.put(4, "four", expiry);

    EXPECT_TRUE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
    EXPECT_TRUE(cache.contains(3));
    EXPECT_TRUE(cache.contains(4));
}

TEST(TtlCacheTest, GetReturnsNullptrAfterExpiry)
{
    TtlCache<int, int> cache(10);
    auto shortExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);

    cache.put(1, 100, shortExpiry);

    EXPECT_TRUE(cache.contains(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(cache.contains(1));
    EXPECT_EQ(cache.get(1), nullptr);
}

TEST(TtlCacheTest, ExpiredElementIsRemovedOnGet)
{
    TtlCache<int, int> cache(10);
    auto shortExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);

    cache.put(1, 100, shortExpiry);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int* val = cache.get(1);

    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.contains(1));
}

TEST(TtlCacheTest, ContainsReturnsFalseAfterExpiry)
{
    TtlCache<int, int> cache(10);
    auto shortExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);

    cache.put(1, 100, shortExpiry);

    EXPECT_TRUE(cache.contains(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(cache.contains(1));
}

TEST(TtlCacheTest, ExpiredElementsCanBeReplaced)
{
    TtlCache<int, int> cache(10);
    auto shortExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    auto longExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put(1, 100, shortExpiry);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    cache.put(1, 200, longExpiry);

    int* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 200);
}

TEST(TtlCacheTest, RespectsMaxCapacity)
{
    TtlCache<int, int> cache(5);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    for (int i = 0; i < 10; ++i)
    {
        cache.put(i, i * 100, expiry);
        EXPECT_LE(cache.size(), 5UL);
    }

    EXPECT_EQ(cache.size(), 5);
}

TEST(TtlCacheTest, FullReturnsTrueWhenCapacityReached)
{
    TtlCache<int, int> cache(3);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    EXPECT_FALSE(cache.full());

    cache.put(1, 100, expiry);
    cache.put(2, 200, expiry);
    cache.put(3, 300, expiry);

    EXPECT_TRUE(cache.full());

    cache.erase(2);
    EXPECT_FALSE(cache.full());
}

TEST(TtlCacheTest, ResizeClearsAndChangesCapacity)
{
    TtlCache<int, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    for (int i = 0; i < 5; ++i)
    {
        cache.put(i, i * 100, expiry);
    }

    EXPECT_EQ(cache.size(), 5);
    EXPECT_EQ(cache.capacity(), 10);

    cache.resize(20);

    EXPECT_EQ(cache.size(), 0);
    EXPECT_EQ(cache.capacity(), 20);
    EXPECT_TRUE(cache.empty());
}

TEST(TtlCacheTest, ResizeThrowsOnZero)
{
    TtlCache<int, int> cache(10);
    EXPECT_THROW(cache.resize(0), std::invalid_argument);
}

TEST(TtlCacheTest, MoveConstructor)
{
    TtlCache<int, std::string> cache1(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache1.put(1, "one", expiry);
    cache1.put(2, "two", expiry);

    TtlCache<int, std::string> cache2(std::move(cache1));

    EXPECT_EQ(cache2.size(), 2);
    EXPECT_EQ(cache2.capacity(), 10);
    EXPECT_TRUE(cache2.contains(1));
    EXPECT_TRUE(cache2.contains(2));

    EXPECT_EQ(cache1.size(), 0);
}

TEST(TtlCacheTest, MoveAssignment)
{
    TtlCache<int, std::string> cache1(10);
    TtlCache<int, std::string> cache2(5);

    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache1.put(1, "one", expiry);
    cache1.put(2, "two", expiry);

    cache2 = std::move(cache1);

    EXPECT_EQ(cache2.size(), 2);
    EXPECT_EQ(cache2.capacity(), 10);
    EXPECT_TRUE(cache2.contains(1));
    EXPECT_TRUE(cache2.contains(2));
}

TEST(TtlCacheTest, WorksWithStringKeys)
{
    TtlCache<std::string, int> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    cache.put("key1", 100, expiry);
    cache.put("key2", 200, expiry);

    int* val = cache.get("key1");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 100);

    EXPECT_TRUE(cache.contains("key2"));
    EXPECT_FALSE(cache.contains("key3"));
}

TEST(TtlCacheTest, WorksWithCustomObjects)
{
    struct TestObject
    {
        int id;
        std::string name;

        bool operator==(const TestObject& other) const
        {
            return id == other.id && name == other.name;
        }
    };

    TtlCache<int, TestObject> cache(10);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    TestObject obj{42, "answer"};
    cache.put(1, std::move(obj), expiry);

    TestObject* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->id, 42);
    EXPECT_EQ(val->name, "answer");
}

TEST(TtlCacheTest, StressTestManyOperations)
{
    TtlCache<int, int> cache(1000);
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    for (int i = 0; i < 10000; ++i)
    {
        cache.put(i % 2000, i, expiry);

        if (i % 100 == 0)
        {
            cache.get(i % 2000);
        }

        if (i % 500 == 0)
        {
            cache.erase(i % 2000);
        }
    }

    EXPECT_LE(cache.size(), 1000UL);
}

TEST(TtlCacheTest, MixedOperations)
{
    TtlCache<int, int> cache(50);
    auto longExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    auto shortExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);

    for (int i = 0; i < 100; ++i)
    {
        cache.put(i, i, (i % 2 == 0) ? longExpiry : shortExpiry);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int found = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (cache.get(i) != nullptr)
        {
            found++;
            EXPECT_EQ(i % 2, 0);
        }
    }

    EXPECT_GT(found, 0);
    EXPECT_LT(found, 100);
}
