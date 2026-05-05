#include <gtest/gtest.h>
#include <gtest/gtest-param-test.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <set>
#include <memory>

#include <casket/types/flat_hash_table.hpp>

using namespace casket;

struct TestStruct
{
    int id;
    std::string name;

    TestStruct(int i = 0, const std::string& n = "")
        : id(i)
        , name(n)
    {
    }

    bool operator==(const TestStruct& other) const
    {
        return id == other.id && name == other.name;
    }
};

namespace std
{
template <>
struct hash<TestStruct>
{
    size_t operator()(const TestStruct& ts) const
    {
        return hash<int>()(ts.id) ^ (hash<string>()(ts.name) << 1);
    }
};
} // namespace std

template <typename Map, typename Key>
auto getValueOrEmpty(const Map& map, const Key& key) -> decltype(*map.find(key))
{
    auto ptr = map.find(key);
    static typename std::decay_t<decltype(*map.find(key))> empty{};
    return ptr ? *ptr : empty;
}

template <typename T>
class FlatHashMapTest : public ::testing::Test
{
protected:
    using MapType = T;
    MapType map;

    void SetUp() override
    {
        map = MapType(1024);
    }
};

using ProbingStrategies = ::testing::Types<LinearHashMap<int, std::string>, RobinHoodHashMap<int, std::string>,
                                           QuadraticHashMap<int, std::string>>;

TYPED_TEST_SUITE(FlatHashMapTest, ProbingStrategies);

TYPED_TEST(FlatHashMapTest, EmptyMapInitially)
{
    ASSERT_EQ(this->map.size(), 0);
    ASSERT_GT(this->map.capacity(), 0);
    ASSERT_FLOAT_EQ(this->map.loadFactor(), 0.0f);
}

TYPED_TEST(FlatHashMapTest, InsertAndFind)
{
    this->map.insert(1, "one");
    this->map.insert(2, "two");

    ASSERT_EQ(this->map.size(), 2);

    auto* val1 = this->map.find(1);
    auto* val2 = this->map.find(2);
    auto* val3 = this->map.find(3);

    ASSERT_NE(val1, nullptr);
    ASSERT_NE(val2, nullptr);
    ASSERT_EQ(val3, nullptr);

    ASSERT_EQ(*val1, "one");
    ASSERT_EQ(*val2, "two");
}

TYPED_TEST(FlatHashMapTest, UpdateExistingKey)
{
    this->map.insert(1, "one");
    this->map.insert(1, "uno");

    ASSERT_EQ(this->map.size(), 1);

    auto* val = this->map.find(1);
    ASSERT_NE(val, nullptr);
    ASSERT_EQ(*val, "uno");
}

TYPED_TEST(FlatHashMapTest, EraseKey)
{
    this->map.insert(1, "one");
    this->map.insert(2, "two");
    this->map.insert(3, "three");

    ASSERT_EQ(this->map.size(), 3);

    bool erased = this->map.erase(2);
    ASSERT_TRUE(erased);
    ASSERT_EQ(this->map.size(), 2);

    auto* val = this->map.find(2);
    ASSERT_EQ(val, nullptr);

    ASSERT_NE(this->map.find(1), nullptr);
    ASSERT_NE(this->map.find(3), nullptr);
}

TYPED_TEST(FlatHashMapTest, EraseNonExistentKey)
{
    ASSERT_FALSE(this->map.erase(999));
    ASSERT_EQ(this->map.size(), 0);
}

TYPED_TEST(FlatHashMapTest, ClearMap)
{
    for (int i = 0; i < 100; ++i)
    {
        this->map.insert(i, "value_" + std::to_string(i));
    }

    ASSERT_EQ(this->map.size(), 100);

    this->map.clear();

    ASSERT_EQ(this->map.size(), 0);
    for (int i = 0; i < 100; ++i)
    {
        ASSERT_EQ(this->map.find(i), nullptr);
    }
}

TYPED_TEST(FlatHashMapTest, ContainsMethod)
{
    this->map.insert(1, "one");
    this->map.insert(2, "two");

    ASSERT_TRUE(this->map.contains(1));
    ASSERT_TRUE(this->map.contains(2));
    ASSERT_FALSE(this->map.contains(3));
}

TYPED_TEST(FlatHashMapTest, EmptyMethod)
{
    ASSERT_TRUE(this->map.empty());
    this->map.insert(1, "one");
    ASSERT_FALSE(this->map.empty());
}

TEST(FlatHashMapTypesTest, StringKeys)
{
    LinearHashMap<std::string, int> map;

    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);

    auto* v1 = map.find(std::string("one"));
    auto* v2 = map.find(std::string("two"));
    auto* v3 = map.find(std::string("three"));
    auto* v4 = map.find(std::string("four"));

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);
    ASSERT_NE(v3, nullptr);
    ASSERT_EQ(v4, nullptr);

    ASSERT_EQ(*v1, 1);
    ASSERT_EQ(*v2, 2);
    ASSERT_EQ(*v3, 3);
}

TEST(FlatHashMapTypesTest, CustomStruct)
{
    RobinHoodHashMap<TestStruct, std::string> map;

    TestStruct ts1(1, "Alice");
    TestStruct ts2(2, "Bob");

    map.insert(ts1, "Engineer");
    map.insert(ts2, "Manager");

    auto* v1 = map.find(ts1);
    auto* v2 = map.find(ts2);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);

    ASSERT_EQ(*v1, "Engineer");
    ASSERT_EQ(*v2, "Manager");
}

TEST(FlatHashMapTypesTest, CopyValue)
{
    LinearHashMap<int, std::string> map;
    std::string value = "hello";

    map.insert(1, value);
    value = "world";

    auto* found = map.find(1);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(*found, "hello");
}

TEST(FlatHashMapTypesTest, MoveValue)
{
    LinearHashMap<int, std::string> map;
    std::string value = "hello";

    map.insert(1, std::move(value));

    auto* found = map.find(1);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(*found, "hello");
}

TEST(FlatHashMapTypesTest, UniquePtrValue)
{
    LinearHashMap<int, std::unique_ptr<int>> map;
    map.insert(1, std::make_unique<int>(42));

    const std::unique_ptr<int>* found = map.find(1);
    ASSERT_NE(found, nullptr);
    ASSERT_NE(*found, nullptr);
    ASSERT_EQ(**found, 42);
}

TYPED_TEST(FlatHashMapTest, InsertManyElements)
{
    const int count = 10000;

    for (int i = 0; i < count; ++i)
    {
        this->map.insert(i, "value_" + std::to_string(i));
    }

    ASSERT_EQ(this->map.size(), count);

    for (int i = 0; i < count; ++i)
    {
        auto* val = this->map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
        ASSERT_EQ(*val, "value_" + std::to_string(i));
    }
}

TYPED_TEST(FlatHashMapTest, InsertAfterRehash)
{
    const int count = 20000;

    for (int i = 0; i < count; ++i)
    {
        this->map.insert(i, "value_" + std::to_string(i));
    }

    ASSERT_EQ(this->map.size(), count);

    for (int i = 0; i < count; ++i)
    {
        auto* val = this->map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
    }
}

TYPED_TEST(FlatHashMapTest, ReserveCapacity)
{
    size_t initialCapacity = this->map.capacity();
    size_t newCapacity = initialCapacity * 2;

    this->map.reserve(newCapacity);

    ASSERT_GE(this->map.capacity(), newCapacity);
}

TEST(RobinHoodTest, MaxProbeDistance)
{
    RobinHoodHashMap<int, std::string> map(64);

    for (int i = 0; i < 55; ++i)
    {
        map.insert(i, "value");
    }

    uint32_t maxDist = map.maxProbeDistance();
    ASSERT_LE(maxDist, 15);
}

TEST(RobinHoodTest, MaxProbeDistanceAfterRehash)
{
    RobinHoodHashMap<int, std::string> map(32);

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, "value");
    }

    uint32_t maxDist = map.maxProbeDistance();
    ASSERT_LE(maxDist, 10);
}

TEST(RobinHoodTest, HighLoadFactorStability)
{
    RobinHoodHashMap<int, int> map(1024, 0.95f);

    const int count = 970; // 95% for 1024

    for (int i = 0; i < count; ++i)
    {
        map.insert(i, i);
    }

    for (int i = 0; i < count; ++i)
    {
        ASSERT_NE(map.find(i), nullptr) << "Missing key: " << i;
    }

    ASSERT_LE(map.maxProbeDistance(), 50);
}

TEST(ProbingTest, LinearProbingWorks)
{
    LinearHashMap<int, std::string> map(8);

    for (int i = 0; i < 8; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for (int i = 0; i < 8; ++i)
    {
        auto* val = map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
        ASSERT_EQ(*val, std::to_string(i));
    }
}

TEST(ProbingTest, QuadraticProbingWorks)
{
    QuadraticHashMap<int, std::string> map(16);

    for (int i = 0; i < 10; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for (int i = 0; i < 10; ++i)
    {
        auto* val = map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
        ASSERT_EQ(*val, std::to_string(i));
    }
}

TEST(ProbingTest, RobinHoodProbingWorks)
{
    RobinHoodHashMap<int, std::string> map(16);

    for (int i = 0; i < 14; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for (int i = 0; i < 14; ++i)
    {
        auto* val = map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
        ASSERT_EQ(*val, std::to_string(i));
    }

    ASSERT_LE(map.maxProbeDistance(), 10);
}

TYPED_TEST(FlatHashMapTest, MinimalCapacityMap)
{
    TypeParam map(1);
    map.insert(1, "one");
    ASSERT_EQ(map.size(), 1);
    ASSERT_NE(map.find(1), nullptr);

    map.insert(2, "two");
    ASSERT_EQ(map.size(), 2);
    ASSERT_NE(map.find(2), nullptr);
}

TYPED_TEST(FlatHashMapTest, OverwriteAfterErase)
{
    this->map.insert(1, "first");
    this->map.erase(1);
    this->map.insert(1, "second");

    auto* val = this->map.find(1);
    ASSERT_NE(val, nullptr);
    ASSERT_EQ(*val, "second");
}

TYPED_TEST(FlatHashMapTest, ManyCollisions)
{
    TypeParam map(16);

    for (int i = 0; i < 15; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for (int i = 0; i < 15; ++i)
    {
        auto* val = map.find(i);
        ASSERT_NE(val, nullptr) << "Missing key: " << i;
        ASSERT_EQ(*val, std::to_string(i));
    }
}

TYPED_TEST(FlatHashMapTest, InsertWithDifferentValueTypes)
{
    this->map.insert(1, "one");

    std::string two = "two";
    this->map.insert(2, two);

    std::string three = "three";
    this->map.insert(3, std::move(three));

    ASSERT_EQ(this->map.size(), 3);
    ASSERT_TRUE(this->map.contains(1));
    ASSERT_TRUE(this->map.contains(2));
    ASSERT_TRUE(this->map.contains(3));
}

TEST(MoveTest, MoveConstructor)
{
    LinearHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    LinearHashMap<int, std::string> map2 = std::move(map1);

    ASSERT_EQ(map2.size(), 2);
    ASSERT_NE(map2.find(1), nullptr);
    ASSERT_NE(map2.find(2), nullptr);
    ASSERT_EQ(*map2.find(1), "one");
    ASSERT_EQ(*map2.find(2), "two");
}

TEST(MoveTest, MoveAssignment)
{
    LinearHashMap<int, std::string> map1;
    map1.insert(1, "one");

    LinearHashMap<int, std::string> map2;
    map2.insert(2, "two");

    map2 = std::move(map1);

    ASSERT_EQ(map2.size(), 1);
    ASSERT_NE(map2.find(1), nullptr);
    ASSERT_EQ(map2.find(2), nullptr);
    ASSERT_EQ(*map2.find(1), "one");
}

TEST(MoveTest, SelfMoveAssignment)
{
    LinearHashMap<int, std::string> map;
    map.insert(1, "one");

    map = std::move(map);

    ASSERT_EQ(map.size(), 1);
    ASSERT_NE(map.find(1), nullptr);
}
