#include <gtest/gtest.h>
#include <functional>
#include <casket/types/pooled_queue.hpp>

using namespace casket;

struct TestItem
{
    int value;
    char data[32];
    
    TestItem() : value(0) {}
    explicit TestItem(int v) : value(v) {}
    TestItem(int v, char c) : value(v) { data[0] = c; }
    
    TestItem(const TestItem&) = default;
    TestItem(TestItem&&) = default;
    TestItem& operator=(const TestItem&) = default;
    TestItem& operator=(TestItem&&) = default;
};

TEST(PooledQueueTest, PushPop)
{
    StrictPooledQueue<int> queue(10);
    
    queue.push(42);
    queue.push(100);
    
    EXPECT_EQ(queue.front(), 42);
    queue.pop();
    EXPECT_EQ(queue.front(), 100);
    queue.pop();
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, PushMove)
{
    StrictPooledQueue<std::string> queue(10);
    
    std::string str = "hello";
    queue.push(std::move(str));
    
    EXPECT_EQ(queue.front(), "hello");
    EXPECT_TRUE(str.empty());
    queue.pop();
}

TEST(PooledQueueTest, Emplace)
{
    StrictPooledQueue<TestItem> queue(10);
    
    queue.emplace(42);
    queue.emplace(100, 'X');
    
    EXPECT_EQ(queue.front().value, 42);
    queue.pop();
    
    EXPECT_EQ(queue.front().value, 100);
    EXPECT_EQ(queue.front().data[0], 'X');
    queue.pop();
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, Front)
{
    StrictPooledQueue<int> queue(10);
    
    queue.push(10);
    queue.push(20);
    
    EXPECT_EQ(queue.front(), 10);
    queue.pop();
    EXPECT_EQ(queue.front(), 20);
}

TEST(PooledQueueTest, FrontConst)
{
    StrictPooledQueue<int> mutableQueue(10);
    mutableQueue.push(10);
    const auto& constQueue = mutableQueue;
    EXPECT_EQ(constQueue.front(), 10);
}

TEST(PooledQueueTest, Empty)
{
    StrictPooledQueue<int> queue(10);
    
    EXPECT_TRUE(queue.empty());
    
    queue.push(1);
    EXPECT_FALSE(queue.empty());
    
    queue.pop();
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, Size)
{
    StrictPooledQueue<int> queue(10);
    
    EXPECT_EQ(queue.size(), 0);
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    EXPECT_EQ(queue.size(), 3);
    
    queue.pop();
    EXPECT_EQ(queue.size(), 2);
    
    queue.pop();
    queue.pop();
    EXPECT_EQ(queue.size(), 0);
}

TEST(PooledQueueTest, Clear)
{
    StrictPooledQueue<int> queue(10);
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    EXPECT_EQ(queue.size(), 3);
    
    queue.clear();
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(PooledQueueTest, Swap)
{
    StrictPooledQueue<int> queue1(10);
    StrictPooledQueue<int> queue2(10);
    
    queue1.push(1);
    queue1.push(2);
    queue2.push(3);
    queue2.push(4);
    
    queue1.swap(queue2);
    
    EXPECT_EQ(queue1.front(), 3);
    queue1.pop();
    EXPECT_EQ(queue1.front(), 4);
    queue1.pop();
    
    EXPECT_EQ(queue2.front(), 1);
    queue2.pop();
    EXPECT_EQ(queue2.front(), 2);
    queue2.pop();
}

TEST(PooledQueueTest, PoolSizeSmall)
{
    StrictPooledQueue<int> queue(2);
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.push(4);
    
    EXPECT_EQ(queue.size(), 2);
    
    EXPECT_EQ(queue.front(), 1);
    queue.pop();
    EXPECT_EQ(queue.front(), 2);
    queue.pop();

    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, ExpandablePoolPolicy)
{
    ExpandPooledQueue<int> queue(2);
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.push(4);
    
    EXPECT_EQ(queue.size(), 4);
    
    EXPECT_EQ(queue.front(), 1);
    queue.pop();
    EXPECT_EQ(queue.front(), 2);
    queue.pop();
    EXPECT_EQ(queue.front(), 3);
    queue.pop();
    EXPECT_EQ(queue.front(), 4);
    queue.pop();
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, ComplexTypeCopy)
{
    StrictPooledQueue<TestItem> queue(10);
    
    TestItem item1(42, 'A');
    TestItem item2(100, 'B');
    
    queue.push(item1);
    queue.push(item2);
    
    EXPECT_EQ(queue.front().value, 42);
    EXPECT_EQ(queue.front().data[0], 'A');
    queue.pop();
    
    EXPECT_EQ(queue.front().value, 100);
    EXPECT_EQ(queue.front().data[0], 'B');
    queue.pop();
}

TEST(PooledQueueTest, ComplexTypeMove)
{
    StrictPooledQueue<TestItem> queue(10);
    
    queue.emplace(42, 'A');
    queue.emplace(100, 'B');
    
    EXPECT_EQ(queue.front().value, 42);
    EXPECT_EQ(queue.front().data[0], 'A');
    queue.pop();
    
    EXPECT_EQ(queue.front().value, 100);
    EXPECT_EQ(queue.front().data[0], 'B');
    queue.pop();
}

TEST(PooledQueueTest, PointerType)
{
    StrictPooledQueue<int*> queue(10);
    
    int a = 10, b = 20;
    queue.push(&a);
    queue.push(&b);
    
    EXPECT_EQ(queue.front(), &a);
    EXPECT_EQ(*queue.front(), 10);
    queue.pop();
    
    EXPECT_EQ(queue.front(), &b);
    EXPECT_EQ(*queue.front(), 20);
    queue.pop();
}

TEST(PooledQueueTest, ReferenceWrapper)
{
    int dummy = 0;
    StrictPooledQueue<std::reference_wrapper<int>> queue(10, std::ref(dummy));
    
    int a = 10, b = 20;
    queue.push(std::ref(a));
    queue.push(std::ref(b));
    
    EXPECT_EQ(queue.front().get(), 10);
    queue.front().get() = 100;
    EXPECT_EQ(a, 100);
    queue.pop();
    
    EXPECT_EQ(queue.front().get(), 20);
    queue.pop();
}

TEST(PooledQueueTest, StressTest)
{
    ExpandPooledQueue<int> queue(100);
    
    for (int i = 0; i < 1000; ++i)
    {
        queue.push(i);
    }
    
    EXPECT_EQ(queue.size(), 1000);
    
    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(queue.front(), i);
        queue.pop();
    }
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, InterleavedPushPop)
{
    StrictPooledQueue<int> queue(10);
    
    queue.push(1);
    queue.push(2);
    
    EXPECT_EQ(queue.front(), 1);
    queue.pop();
    
    queue.push(3);
    queue.push(4);
    
    EXPECT_EQ(queue.front(), 2);
    queue.pop();
    EXPECT_EQ(queue.front(), 3);
    queue.pop();
    EXPECT_EQ(queue.front(), 4);
    queue.pop();
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, LargeObjects)
{
    struct LargeObject
    {
        char data[4096];
        int id;
        
        LargeObject() : id(0) {}
        explicit LargeObject(int i) : id(i) {}
    };
    
    StrictPooledQueue<LargeObject> queue(100);
    
    for (int i = 0; i < 100; ++i)
    {
        queue.emplace(i);
    }
    
    EXPECT_EQ(queue.size(), 100);
    
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(queue.front().id, i);
        queue.pop();
    }
}

TEST(PooledQueueTest, NonDefaultConstructible)
{
    struct NonDefaultConstructible
    {
        int value;
        
        NonDefaultConstructible() = delete;
        explicit NonDefaultConstructible(int v) : value(v) {}
        
        NonDefaultConstructible(const NonDefaultConstructible&) = default;
        NonDefaultConstructible(NonDefaultConstructible&&) = default;
    };
    
    StrictPooledQueue<NonDefaultConstructible> queue(10, 0);
    
    queue.emplace(42);
    queue.emplace(100);
    
    EXPECT_EQ(queue.front().value, 42);
    queue.pop();
    EXPECT_EQ(queue.front().value, 100);
    queue.pop();
    
    EXPECT_TRUE(queue.empty());
}

TEST(PooledQueueTest, PoolStats)
{
    StrictPooledQueue<int> queue(10, 1);
    
    auto stats = queue.poolStats();
    EXPECT_EQ(stats.first, 10);
    EXPECT_EQ(stats.second, 0);
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    stats = queue.poolStats();
    EXPECT_EQ(stats.first, 10);
    EXPECT_EQ(stats.second, 3);
    
    queue.pop();
    
    stats = queue.poolStats();
    EXPECT_EQ(stats.first, 10);
    EXPECT_EQ(stats.second, 2);
    
    queue.clear();
    stats = queue.poolStats();
    EXPECT_EQ(stats.first, 10);
    EXPECT_EQ(stats.second, 0);
}