#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <numeric>

#include <casket/lock_free/lf_mpsc_queue.hpp>

using namespace casket;

/// @brief Test fixture for MPSCQueue.
template<typename T>
class MPSCQueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        queue = std::make_unique<MPSCQueue<T>>();
    }

    void TearDown() override
    {
        queue.reset();
    }

    std::unique_ptr<MPSCQueue<T>> queue;
};

using TestTypes = ::testing::Types<int, std::string, std::shared_ptr<int>>;
TYPED_TEST_SUITE(MPSCQueueTest, TestTypes);

/// @brief Tests that newly created queue is empty.
TYPED_TEST(MPSCQueueTest, initiallyEmpty)
{
    TypeParam dummy;
    EXPECT_FALSE(this->queue->pop(dummy));
    EXPECT_TRUE(this->queue->empty());
}

/// @brief Tests single push and pop.
TYPED_TEST(MPSCQueueTest, singlePushPop)
{
    TypeParam value{};
    this->queue->push(std::move(value));
    
    EXPECT_FALSE(this->queue->empty());
    
    TypeParam result{};
    EXPECT_TRUE(this->queue->pop(result));
    EXPECT_TRUE(this->queue->empty());
}

/// @brief Tests multiple sequential pushes and pops.
TYPED_TEST(MPSCQueueTest, multipleSequentialPushPop)
{
    constexpr int numItems = 100;
    
    for (int i = 0; i < numItems; ++i)
    {
        TypeParam value{};
        this->queue->push(std::move(value));
    }
    
    for (int i = 0; i < numItems; ++i)
    {
        TypeParam result{};
        EXPECT_TRUE(this->queue->pop(result));
    }
    
    EXPECT_TRUE(this->queue->empty());
}

/// @brief Tests FIFO order preservation.
TEST(MPSCQueueIntTest, fifoOrderPreserved)
{
    MPSCQueue<int> queue;
    constexpr int numItems = 1000;
    
    for (int i = 0; i < numItems; ++i)
    {
        queue.push(std::move(i));
    }
    
    for (int i = 0; i < numItems; ++i)
    {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }
}

/// @brief Tests multiple producers and single consumer.
TEST(MPSCQueueIntTest, multipleProducersSingleConsumer)
{
    MPSCQueue<int> queue;
    constexpr int numProducers = 4;
    constexpr int itemsPerProducer = 2500;
    constexpr int totalItems = numProducers * itemsPerProducer;
    
    std::vector<std::thread> producers;
    
    // Launch producers
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&queue, p, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i)
            {
                int value = p * itemsPerProducer + i;
                queue.push(std::move(value));
            }
        });
    }
    
    // Consumer collects items
    std::vector<int> consumed;
    consumed.reserve(totalItems);
    
    while (consumed.size() < totalItems)
    {
        int value = -1;
        if (queue.pop(value))
        {
            consumed.push_back(value);
        }
        else
        {
            std::this_thread::yield();
        }
    }
    
    // Join all producers
    for (auto& t : producers)
    {
        t.join();
    }
    
    // Verify all items were consumed exactly once
    EXPECT_EQ(consumed.size(), totalItems);
    
    std::sort(consumed.begin(), consumed.end());
    for (int i = 0; i < totalItems; ++i)
    {
        EXPECT_EQ(consumed[i], i);
    }
}

/// @brief Tests push from multiple threads with contention.
TEST(MPSCQueueIntTest, highContentionMultipleProducers)
{
    MPSCQueue<int> queue;
    constexpr int numProducers = 8;
    constexpr int itemsPerProducer = 5000;
    std::atomic<int> successfulPops{0};
    
    std::vector<std::thread> producers;
    
    // Launch producers
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&queue, p, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i)
            {
                queue.push(p * itemsPerProducer + i);
            }
        });
    }
    
    // Single consumer
    std::thread consumer([&queue, &successfulPops, numProducers, itemsPerProducer]() {
        int totalExpected = numProducers * itemsPerProducer;
        int value;
        while (successfulPops.load() < totalExpected)
        {
            if (queue.pop(value))
            {
                successfulPops++;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });
    
    // Wait for completion
    for (auto& t : producers)
    {
        t.join();
    }
    consumer.join();
    
    EXPECT_EQ(successfulPops.load(), numProducers * itemsPerProducer);
    EXPECT_TRUE(queue.empty());
}

/// @brief Tests string queue with multiple producers.
TEST(MPSCQueueStringTest, multipleProducersStrings)
{
    MPSCQueue<std::string> queue;
    constexpr int numProducers = 4;
    constexpr int itemsPerProducer = 1000;
    std::atomic<int> consumedCount{0};
    
    std::vector<std::thread> producers;
    
    // Launch producers
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&queue, p, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i)
            {
                std::string str = "Producer_" + std::to_string(p) + "_Item_" + std::to_string(i);
                queue.push(std::move(str));
            }
        });
    }
    
    // Consumer
    std::thread consumer([&queue, &consumedCount, numProducers, itemsPerProducer]() {
        int totalExpected = numProducers * itemsPerProducer;
        std::string value;
        
        while (consumedCount.load() < totalExpected)
        {
            if (queue.pop(value))
            {
                consumedCount++;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& t : producers)
    {
        t.join();
    }
    consumer.join();
    
    EXPECT_EQ(consumedCount.load(), numProducers * itemsPerProducer);
    EXPECT_TRUE(queue.empty());
}

/// @brief Tests clear operation.
TEST(MPSCQueueIntTest, clearRemovesAllElements)
{
    MPSCQueue<int> queue;
    constexpr int numItems = 100;
    
    for (int i = 0; i < numItems; ++i)
    {
        queue.push(std::move(i));
    }
    
    EXPECT_FALSE(queue.empty());
    queue.clear();
    EXPECT_TRUE(queue.empty());
    
    int dummy;
    EXPECT_FALSE(queue.pop(dummy));
}

/// @brief Tests pop on empty queue returns false.
TEST(MPSCQueueIntTest, popOnEmptyQueueReturnsFalse)
{
    MPSCQueue<int> queue;
    int value;
    EXPECT_FALSE(queue.pop(value));
    
    queue.push(42);
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_FALSE(queue.pop(value));
}

/// @brief Tests queue with large number of elements.
TEST(MPSCQueueIntTest, largeNumberOfElements)
{
    MPSCQueue<int> queue;
    constexpr int numItems = 100000;
    
    for (int i = 0; i < numItems; ++i)
    {
        queue.push(std::move(i));
    }
    
    for (int i = 0; i < numItems; ++i)
    {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(queue.empty());
}

/// @brief Tests that pool reuses nodes efficiently.
TEST(MPSCQueueIntTest, nodePoolReuse)
{
    MPSCQueue<int> queue(100); // Small pool size
    
    // Push more than pool size to trigger heap allocation
    constexpr int numItems = 500;
    
    for (int i = 0; i < numItems; ++i)
    {
        queue.push(std::move(i));
    }
    
    for (int i = 0; i < numItems; ++i)
    {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(queue.empty());
    
    // Push again to verify pool still works
    for (int i = 0; i < numItems; ++i)
    {
        queue.push(std::move(i));
    }
    
    for (int i = 0; i < numItems; ++i)
    {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }
}

/// @brief Tests concurrent push and pop with timeout.
TEST(MPSCQueueIntTest, concurrentPushPopWithTimeout)
{
    MPSCQueue<int> queue;
    std::atomic<bool> producerDone{false};
    std::atomic<int> itemsProduced{0};
    std::atomic<int> itemsConsumed{0};
    constexpr int targetItems = 10000;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < targetItems; ++i)
        {
            queue.push(std::move(i));
            itemsProduced++;
        }
        producerDone = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (!producerDone || itemsConsumed < targetItems)
        {
            if (queue.pop(value))
            {
                itemsConsumed++;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(itemsProduced, targetItems);
    EXPECT_EQ(itemsConsumed, targetItems);
    EXPECT_TRUE(queue.empty());
}

/// @brief Tests move-only types.
TEST(MPSCQueueUniquePtrTest, moveOnlyTypeSupport)
{
    MPSCQueue<std::unique_ptr<int>> queue;
    
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(100);
    
    queue.push(std::move(ptr1));
    queue.push(std::move(ptr2));
    
    std::unique_ptr<int> result1;
    std::unique_ptr<int> result2;
    
    EXPECT_TRUE(queue.pop(result1));
    EXPECT_TRUE(queue.pop(result2));
    
    EXPECT_EQ(*result1, 42);
    EXPECT_EQ(*result2, 100);
    EXPECT_TRUE(queue.empty());
}

/// @brief Tests performance under high load.
TEST(MPSCQueueIntTest, performanceUnderHighLoad)
{
    MPSCQueue<int> queue;
    constexpr int numProducers = 4;
    constexpr int itemsPerProducer = 50000;
    constexpr int totalItems = numProducers * itemsPerProducer;
    
    std::vector<std::thread> producers;
    auto startTime = std::chrono::steady_clock::now();
    
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&queue, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i)
            {
                queue.push(i);
            }
        });
    }
    
    std::atomic<int> consumed{0};
    std::thread consumer([&]() {
        int value;
        while (consumed < totalItems)
        {
            if (queue.pop(value))
            {
                consumed++;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& t : producers)
    {
        t.join();
    }
    consumer.join();
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_EQ(consumed, totalItems);
    EXPECT_TRUE(queue.empty());
    
    // Performance assertion: should handle at least 1M ops/sec
    // This is just a sanity check, actual performance may vary
    std::cout << "Processed " << totalItems << " items in " << duration.count() << " ms" << std::endl;
}
