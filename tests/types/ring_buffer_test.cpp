#include <gtest/gtest.h>
#include <casket/types/ring_buffer.hpp>

using namespace casket;

template <typename T>
void fillBuffer(RingBuffer<T>& buffer, const std::vector<T>& data)
{
    for (const auto& item : data)
    {
        buffer.push(item);
    }
}

class RingBufferTest : public ::testing::Test
{
};

// Constructor tests
TEST_F(RingBufferTest, ConstructorThrowsOnZeroCapacity)
{
    EXPECT_THROW(RingBuffer<int> buffer(0), std::invalid_argument);
}

TEST_F(RingBufferTest, ConstructorCreatesCorrectCapacity)
{
    RingBuffer<int> buffer(5);
    EXPECT_EQ(buffer.capacity(), 5);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
}

// Basic operations tests
TEST_F(RingBufferTest, PushAndSize)
{
    RingBuffer<int> buffer(3);

    EXPECT_TRUE(buffer.push(1));
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_FALSE(buffer.empty());

    EXPECT_TRUE(buffer.push(2));
    EXPECT_EQ(buffer.size(), 2);

    EXPECT_TRUE(buffer.push(3));
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_TRUE(buffer.full());
}

TEST_F(RingBufferTest, PushReturnsCorrectValueWhenFull)
{
    RingBuffer<int> buffer(2);

    // First two pushes should return true (had space)
    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.full());

    // Third push should return false (was already full)
    EXPECT_FALSE(buffer.push(3));
    EXPECT_TRUE(buffer.full());
}

TEST_F(RingBufferTest, PopEmptyBuffer)
{
    RingBuffer<int> buffer(3);

    EXPECT_FALSE(buffer.pop()); // Pop without retrieving value
    EXPECT_EQ(buffer.size(), 0);

    int value;
    EXPECT_FALSE(buffer.pop(value)); // Pop with retrieving value
}

TEST_F(RingBufferTest, PopRetrievesCorrectValue)
{
    RingBuffer<int> buffer(3);
    buffer.push(10);
    buffer.push(20);
    buffer.push(30);

    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 10);
    EXPECT_EQ(buffer.size(), 2);

    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 20);
    EXPECT_EQ(buffer.size(), 1);

    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 30);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
}

TEST_F(RingBufferTest, PopWithoutRetrieval)
{
    RingBuffer<int> buffer(3);
    buffer.push(10);
    buffer.push(20);

    EXPECT_TRUE(buffer.pop());
    EXPECT_EQ(buffer.size(), 1);

    EXPECT_TRUE(buffer.pop());
    EXPECT_TRUE(buffer.empty());
}

// Front and back tests
TEST_F(RingBufferTest, FrontAndBackAccess)
{
    RingBuffer<int> buffer(5);

    buffer.push(10);
    EXPECT_EQ(buffer.front(), 10);
    EXPECT_EQ(buffer.back(), 10);

    buffer.push(20);
    EXPECT_EQ(buffer.front(), 10);
    EXPECT_EQ(buffer.back(), 20);

    buffer.push(30);
    EXPECT_EQ(buffer.front(), 10);
    EXPECT_EQ(buffer.back(), 30);
}

TEST_F(RingBufferTest, FrontAndBackThrowWhenEmpty)
{
    RingBuffer<int> buffer(3);

    EXPECT_THROW(buffer.front(), std::out_of_range);
    EXPECT_THROW(buffer.back(), std::out_of_range);

    // Test const versions
    const auto& const_buffer = buffer;
    EXPECT_THROW(const_buffer.front(), std::out_of_range);
    EXPECT_THROW(const_buffer.back(), std::out_of_range);
}

TEST_F(RingBufferTest, FrontAndBackAreModifiable)
{
    RingBuffer<int> buffer(3);
    buffer.push(10);

    buffer.front() = 20;
    EXPECT_EQ(buffer.front(), 20);

    buffer.back() = 30;
    EXPECT_EQ(buffer.back(), 30);
}

// Circular behavior tests
TEST_F(RingBufferTest, CircularWrapAround)
{
    RingBuffer<int> buffer(3);

    // Fill buffer
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    // Remove one element
    buffer.pop();

    // Add new element - should wrap around
    buffer.push(4);

    // Check contents
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_TRUE(buffer.full());
}

TEST_F(RingBufferTest, OverwriteOldestWhenFull)
{
    RingBuffer<int> buffer(3);

    // Fill buffer completely
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    // Buffer is now: [1, 2, 3]
    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 3);

    // Push another element - should overwrite oldest
    buffer.push(4);
    // Buffer should now be: [2, 3, 4]
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.back(), 4);

    // Push another element
    buffer.push(5);
    // Buffer should now be: [3, 4, 5]
    EXPECT_EQ(buffer.front(), 3);
    EXPECT_EQ(buffer.back(), 5);
}

// Size calculation tests
TEST_F(RingBufferTest, SizeCalculation)
{
    RingBuffer<int> buffer(5);

    EXPECT_EQ(buffer.size(), 0);

    for (int i = 1; i <= 3; ++i)
    {
        buffer.push(i);
        EXPECT_EQ(buffer.size(), i);
    }

    for (int i = 1; i <= 2; ++i)
    {
        buffer.pop();
        EXPECT_EQ(buffer.size(), 3 - i);
    }
}

// Clear tests
TEST_F(RingBufferTest, Clear)
{
    RingBuffer<int> buffer(5);

    fillBuffer(buffer, {1, 2, 3, 4, 5});
    EXPECT_EQ(buffer.size(), 5);
    EXPECT_TRUE(buffer.full());

    buffer.clear();
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());

    // Should be able to use after clear
    buffer.push(10);
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer.front(), 10);
}

// Noexcept tests for types with appropriate operations
struct MoveOnly
{
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};

struct ThrowingMove
{
    ThrowingMove() = default;
    ThrowingMove(ThrowingMove&&) noexcept(false)
    {
    }
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false)
    {
        return *this;
    }
};

TEST_F(RingBufferTest, NoexceptPushForNothrowMoveAssignable)
{
    EXPECT_TRUE(noexcept(std::declval<RingBuffer<MoveOnly>&>().push(MoveOnly{})));
    EXPECT_FALSE(noexcept(std::declval<RingBuffer<ThrowingMove>&>().push(ThrowingMove{})));
}

TEST_F(RingBufferTest, NoexceptPopForNothrowMoveConstructible)
{
    RingBuffer<MoveOnly> buffer1(5);
    MoveOnly value1;
    EXPECT_TRUE(noexcept(buffer1.pop(value1)));

    RingBuffer<ThrowingMove> buffer2(5);
    ThrowingMove value2;
    EXPECT_FALSE(noexcept(buffer2.pop(value2)));
}

// Test with custom storage type
TEST_F(RingBufferTest, WithCustomStorage)
{
    // Using std::array as custom storage (requires capacity known at compile time)
    // Note: This is just a demonstration, RingBuffer with std::array would need
    // different constructor
    RingBuffer<int, std::vector<int>> buffer(10);
    EXPECT_EQ(buffer.capacity(), 10);

    for (int i = 0; i < 15; ++i)
    {
        buffer.push(i);
    }

    EXPECT_EQ(buffer.size(), 10);
    EXPECT_TRUE(buffer.full());
}

// Test complex scenario
TEST_F(RingBufferTest, ComplexScenario)
{
    RingBuffer<int> buffer(4);

    // Phase 1: Fill partially
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 3);

    // Phase 2: Remove some
    buffer.pop();
    buffer.pop();
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer.front(), 3);
    EXPECT_EQ(buffer.back(), 3);

    // Phase 3: Fill to capacity with wrap-around
    buffer.push(4);
    buffer.push(5);
    buffer.push(6); // This should cause wrap-around
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.front(), 3);
    EXPECT_EQ(buffer.back(), 6);

    // Phase 4: Overwrite all
    buffer.push(7);
    buffer.push(8);
    buffer.push(9);
    buffer.push(10);
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_EQ(buffer.front(), 7);
    EXPECT_EQ(buffer.back(), 10);

    // Phase 5: Empty completely
    while (!buffer.empty())
    {
        buffer.pop();
    }
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
}

// Test with different types
TEST_F(RingBufferTest, StringType)
{
    RingBuffer<std::string> buffer(3);

    buffer.push("first");
    buffer.push("second");
    buffer.push("third");

    EXPECT_EQ(buffer.front(), "first");
    EXPECT_EQ(buffer.back(), "third");

    // Overwrite
    buffer.push("fourth");
    EXPECT_EQ(buffer.front(), "second");
    EXPECT_EQ(buffer.back(), "fourth");

    std::string value;
    buffer.pop(value);
    EXPECT_EQ(value, "second");
}

TEST_F(RingBufferTest, DoubleType)
{
    RingBuffer<double> buffer(5);

    for (int i = 0; i < 10; ++i)
    {
        buffer.push(i * 1.1);
    }

    // Should have last 5 elements
    EXPECT_EQ(buffer.size(), 5);
    EXPECT_NEAR(buffer.front(), 5 * 1.1, 0.0001);
    EXPECT_NEAR(buffer.back(), 9 * 1.1, 0.0001);
}
