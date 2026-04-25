#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <casket/pack/pack.hpp>

using namespace casket;

class PackerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        buffer_.resize(1024);
        packer_ = std::make_unique<Packer>(buffer_.data(), buffer_.size());
    }

    std::vector<uint8_t> buffer_;
    std::unique_ptr<Packer> packer_;
};

TEST_F(PackerTest, PackNil)
{
    auto result = packer_->packNil();
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 1);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Nil));
}

TEST_F(PackerTest, PackBool)
{
    auto resultTrue = packer_->pack(true);
    ASSERT_TRUE(resultTrue);
    ASSERT_EQ(packer_->position(), 1);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::True));

    packer_->reset();
    auto resultFalse = packer_->pack(false);
    ASSERT_TRUE(resultFalse);
    ASSERT_EQ(packer_->position(), 1);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::False));
}

TEST_F(PackerTest, PackInt8)
{
    int8_t value = -42;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 2);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Int8));
    ASSERT_EQ(*reinterpret_cast<int8_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackInt16)
{
    int16_t value = -12345;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 3);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Int16));
    ASSERT_EQ(*reinterpret_cast<int16_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackInt32)
{
    int32_t value = -123456789;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Int32));
    ASSERT_EQ(*reinterpret_cast<int32_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackInt64)
{
    int64_t value = -123456789012345LL;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 9);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Int64));
    ASSERT_EQ(*reinterpret_cast<int64_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackUInt8)
{
    uint8_t value = 255;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 2);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::UInt8));
    ASSERT_EQ(*reinterpret_cast<uint8_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackUInt16)
{
    uint16_t value = 65535;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 3);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::UInt16));
    ASSERT_EQ(*reinterpret_cast<uint16_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackUInt32)
{
    uint32_t value = 4294967295U;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::UInt32));
    ASSERT_EQ(*reinterpret_cast<uint32_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackUInt64)
{
    uint64_t value = 18446744073709551615ULL;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 9);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::UInt64));
    ASSERT_EQ(*reinterpret_cast<uint64_t*>(&buffer_[1]), value);
}

TEST_F(PackerTest, PackFloat)
{
    float value = 123.456f;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Float32));

    float written;
    memcpy(&written, &buffer_[1], sizeof(float));
    ASSERT_FLOAT_EQ(written, value);
}

TEST_F(PackerTest, PackDouble)
{
    double value = 123.456789;
    auto result = packer_->pack(value);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 9);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Float64));

    double written;
    memcpy(&written, &buffer_[1], sizeof(double));
    ASSERT_DOUBLE_EQ(written, value);
}

TEST_F(PackerTest, PackString8)
{
    std::string str = "Hello";
    auto result = packer_->pack(nonstd::string_view(str));
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 2 + str.size());
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Str8));
    ASSERT_EQ(buffer_[1], static_cast<uint8_t>(str.size()));
    ASSERT_EQ(memcmp(&buffer_[2], str.data(), str.size()), 0);
}

TEST_F(PackerTest, PackString16)
{
    std::string str(300, 'a'); // 300 > 0xFF
    auto result = packer_->pack(nonstd::string_view(str));
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 3 + str.size());
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Str16));

    uint16_t size;
    memcpy(&size, &buffer_[1], sizeof(uint16_t));
    ASSERT_EQ(size, str.size());
    ASSERT_EQ(memcmp(&buffer_[3], str.data(), str.size()), 0);
}

TEST_F(PackerTest, PackString32)
{
    buffer_.resize(70005);
    packer_ = std::make_unique<Packer>(buffer_.data(), buffer_.size());

    std::string str(70000, 'a'); // 70000 > 0xFFFF
    auto result = packer_->pack(nonstd::string_view(str));
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5 + str.size());
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Str32));

    uint32_t size;
    memcpy(&size, &buffer_[1], sizeof(uint32_t));
    ASSERT_EQ(size, str.size());
    ASSERT_EQ(memcmp(&buffer_[5], str.data(), str.size()), 0);
}

TEST_F(PackerTest, PackEmptyString)
{
    std::string str;
    auto result = packer_->pack(nonstd::string_view(str));
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 2);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Str8));
    ASSERT_EQ(buffer_[1], 0);
}

TEST_F(PackerTest, PackArray16)
{
    size_t size = 100;
    auto result = packer_->packArrayStart(size);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 3);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Array16));

    uint16_t written;
    memcpy(&written, &buffer_[1], sizeof(uint16_t));
    ASSERT_EQ(written, size);
}

TEST_F(PackerTest, PackArray32)
{
    size_t size = 70000;
    auto result = packer_->packArrayStart(size);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Array32));

    uint32_t written;
    memcpy(&written, &buffer_[1], sizeof(uint32_t));
    ASSERT_EQ(written, size);
}

TEST_F(PackerTest, PackMap16)
{
    size_t size = 100;
    auto result = packer_->packMapStart(size);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 3);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Map16));

    uint16_t written;
    memcpy(&written, &buffer_[1], sizeof(uint16_t));
    ASSERT_EQ(written, size);
}

TEST_F(PackerTest, PackMap32)
{
    size_t size = 70000;
    auto result = packer_->packMapStart(size);
    ASSERT_TRUE(result);
    ASSERT_EQ(packer_->position(), 5);
    ASSERT_EQ(buffer_[0], static_cast<uint8_t>(TypeTag::Map32));

    uint32_t written;
    memcpy(&written, &buffer_[1], sizeof(uint32_t));
    ASSERT_EQ(written, size);
}

TEST_F(PackerTest, BufferOverflowNil)
{
    std::vector<uint8_t> smallBuffer(0);
    Packer smallPacker(smallBuffer.data(), smallBuffer.size());
    auto result = smallPacker.packNil();
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error(), PackerError::BufferOverflow);
}

TEST_F(PackerTest, BufferOverflowInt32)
{
    std::vector<uint8_t> smallBuffer(4);
    Packer smallPacker(smallBuffer.data(), smallBuffer.size());
    auto result = smallPacker.pack(12345);
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error(), PackerError::BufferOverflow);
}

TEST_F(PackerTest, BufferOverflowString)
{
    std::vector<uint8_t> smallBuffer(10);
    Packer smallPacker(smallBuffer.data(), smallBuffer.size());
    std::string longString(100, 'x');
    auto result = smallPacker.pack(nonstd::string_view(longString));
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error(), PackerError::BufferOverflow);
}

TEST_F(PackerTest, ResetAndReuse)
{
    packer_->pack(42);
    size_t firstSize = packer_->position();
    ASSERT_GT(firstSize, 0);

    packer_->reset();
    ASSERT_EQ(packer_->position(), 0);

    packer_->pack("Hello");
    ASSERT_GT(packer_->position(), 0);
    ASSERT_NE(packer_->position(), firstSize);
}

TEST_F(PackerTest, MultiplePacks)
{
    packer_->packNil();
    packer_->pack(true);
    packer_->pack(42);
    packer_->pack(3.14f);
    packer_->pack("Test");

    size_t expectedSize = 1 + 1 + 5 + 5 + (2 + 4);
    ASSERT_EQ(packer_->position(), expectedSize);
}

TEST_F(PackerTest, CapacityAndRemaining)
{
    size_t capacity = packer_->capacity();
    ASSERT_EQ(capacity, buffer_.size());
    ASSERT_EQ(packer_->remaining(), capacity);

    packer_->pack(42);
    ASSERT_EQ(packer_->remaining(), capacity - packer_->position());
}

TEST_F(PackerTest, DataPointer)
{
    packer_->pack(42);
    const uint8_t* data = packer_->data();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data, buffer_.data());
}

TEST_F(PackerTest, ComplexStructure)
{
    packer_->packArrayStart(2);

    packer_->pack(123);

    packer_->packMapStart(2);

    packer_->pack("name");
    packer_->pack("John");

    packer_->pack("age");
    packer_->pack(30);

    ASSERT_EQ(packer_->position(), 3 + 5 + 3 + (2 + 4) + (2 + 3) + (2 + 3) + (2 + 1) + (2 + 1));
}
