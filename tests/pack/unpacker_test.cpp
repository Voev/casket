#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <casket/pack/packer.hpp>
#include <casket/pack/unpacker.hpp>

using namespace casket;

class PackerUnpackerIntegrationTest : public ::testing::Test
{
protected:
    std::vector<uint8_t> buffer_;
    std::unique_ptr<Packer> packer_;
    std::unique_ptr<Unpacker> unpacker_;

    void SetUp() override
    {
        buffer_.resize(4096);
        packer_ = std::make_unique<Packer>(buffer_.data(), buffer_.size());
    }

    void resetUnpacker()
    {
        unpacker_ = std::make_unique<Unpacker>(buffer_.data(), packer_->position());
    }
};

TEST_F(PackerUnpackerIntegrationTest, BoolTrue)
{
    ASSERT_TRUE(packer_->pack(true));
    resetUnpacker();

    auto result = unpacker_->unpackBool();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), true);
    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, BoolFalse)
{
    ASSERT_TRUE(packer_->pack(false));
    resetUnpacker();

    auto result = unpacker_->unpackBool();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), false);
    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, Int8)
{
    int8_t value = -42;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackInt8();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, Int16)
{
    int16_t value = -12345;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackInt16();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, Int32)
{
    int32_t value = -123456789;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackInt32();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, Int64)
{
    int64_t value = -123456789012345LL;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackInt64();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, UInt8)
{
    uint8_t value = 255;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackUInt8();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, UInt16)
{
    uint16_t value = 65535;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackUInt16();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, UInt32)
{
    uint32_t value = 4294967295U;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackUInt32();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, UInt64)
{
    uint64_t value = 18446744073709551615ULL;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackUInt64();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, Float)
{
    float value = 123.456f;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackFloat();
    ASSERT_TRUE(result.has_value());
    ASSERT_FLOAT_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, Double)
{
    double value = 123.456789;
    ASSERT_TRUE(packer_->pack(value));
    resetUnpacker();

    auto result = unpacker_->unpackDouble();
    ASSERT_TRUE(result.has_value());
    ASSERT_DOUBLE_EQ(result.value(), value);
}

TEST_F(PackerUnpackerIntegrationTest, String8)
{
    std::string str = "Hello, World!";
    ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    resetUnpacker();

    auto result = unpacker_->unpackString();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), str);
}

TEST_F(PackerUnpackerIntegrationTest, String16)
{
    std::string str(300, 'a'); // > 255
    ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    resetUnpacker();

    auto result = unpacker_->unpackString();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), str);
}

TEST_F(PackerUnpackerIntegrationTest, String32)
{
    buffer_.resize(70005);
    packer_ = std::make_unique<Packer>(buffer_.data(), buffer_.size());

    std::string str(70000, 'a'); // > 65535
    ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    resetUnpacker();

    auto result = unpacker_->unpackString();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), str);
}

TEST_F(PackerUnpackerIntegrationTest, EmptyString)
{
    std::string str;
    ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    resetUnpacker();

    auto result = unpacker_->unpackString();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), str);
}

TEST_F(PackerUnpackerIntegrationTest, StringWithSpecialChars)
{
    std::string str = "Hello\n\t\r\0World";
    ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    resetUnpacker();

    auto result = unpacker_->unpackString();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), str.size());
    ASSERT_EQ(memcmp(result.value().data(), str.data(), str.size()), 0);
}

TEST_F(PackerUnpackerIntegrationTest, Array16)
{
    size_t size = 100;
    ASSERT_TRUE(packer_->packArrayStart(size));
    resetUnpacker();

    auto result = unpacker_->unpackArraySize();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), size);
}

TEST_F(PackerUnpackerIntegrationTest, Array32)
{
    size_t size = 70000;
    ASSERT_TRUE(packer_->packArrayStart(size));
    resetUnpacker();

    auto result = unpacker_->unpackArraySize();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), size);
}

TEST_F(PackerUnpackerIntegrationTest, ArrayWithElements)
{
    ASSERT_TRUE(packer_->packArrayStart(3));
    ASSERT_TRUE(packer_->pack(1));
    ASSERT_TRUE(packer_->pack(2));
    ASSERT_TRUE(packer_->pack(3));
    resetUnpacker();

    auto arraySize = unpacker_->unpackArraySize();
    ASSERT_TRUE(arraySize.has_value());
    ASSERT_EQ(arraySize.value(), 3);

    for (int i = 1; i <= 3; ++i)
    {
        auto value = unpacker_->unpackInt32();
        ASSERT_TRUE(value.has_value());
        ASSERT_EQ(value.value(), i);
    }
    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, Map16)
{
    size_t size = 50;
    ASSERT_TRUE(packer_->packMapStart(size));
    resetUnpacker();

    auto result = unpacker_->unpackMapSize();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), size);
}

TEST_F(PackerUnpackerIntegrationTest, Map32)
{
    size_t size = 50000;
    ASSERT_TRUE(packer_->packMapStart(size));
    resetUnpacker();

    auto result = unpacker_->unpackMapSize();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), size);
}

TEST_F(PackerUnpackerIntegrationTest, MapWithElements)
{
    ASSERT_TRUE(packer_->packMapStart(2));

    ASSERT_TRUE(packer_->pack("name"));
    ASSERT_TRUE(packer_->pack("John"));

    ASSERT_TRUE(packer_->pack("age"));
    ASSERT_TRUE(packer_->pack(30));
    resetUnpacker();

    auto mapSize = unpacker_->unpackMapSize();
    ASSERT_TRUE(mapSize.has_value());
    ASSERT_EQ(mapSize.value(), 2);

    auto key1 = unpacker_->unpackString();
    ASSERT_TRUE(key1.has_value());
    ASSERT_EQ(key1.value(), "name");

    auto value1 = unpacker_->unpackString();
    ASSERT_TRUE(value1.has_value());
    ASSERT_EQ(value1.value(), "John");

    auto key2 = unpacker_->unpackString();
    ASSERT_TRUE(key2.has_value());
    ASSERT_EQ(key2.value(), "age");

    auto value2 = unpacker_->unpackInt32();
    ASSERT_TRUE(value2.has_value());
    ASSERT_EQ(value2.value(), 30);

    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, ComplexNestedStructure)
{
    ASSERT_TRUE(packer_->packArrayStart(2));

    ASSERT_TRUE(packer_->pack(12345));

    ASSERT_TRUE(packer_->packArrayStart(3));
    ASSERT_TRUE(packer_->pack(true));
    ASSERT_TRUE(packer_->pack(3.14f));
    ASSERT_TRUE(packer_->pack("nested"));

    resetUnpacker();

    auto outerArraySize = unpacker_->unpackArraySize();
    ASSERT_TRUE(outerArraySize.has_value());
    ASSERT_EQ(outerArraySize.value(), 2);

    auto number = unpacker_->unpackInt32();
    ASSERT_TRUE(number.has_value());
    ASSERT_EQ(number.value(), 12345);

    auto innerArraySize = unpacker_->unpackArraySize();
    ASSERT_TRUE(innerArraySize.has_value());
    ASSERT_EQ(innerArraySize.value(), 3);

    auto boolValue = unpacker_->unpackBool();
    ASSERT_TRUE(boolValue.has_value());
    ASSERT_EQ(boolValue.value(), true);

    auto floatValue = unpacker_->unpackFloat();
    ASSERT_TRUE(floatValue.has_value());
    ASSERT_FLOAT_EQ(floatValue.value(), 3.14f);

    auto stringValue = unpacker_->unpackString();
    ASSERT_TRUE(stringValue.has_value());
    ASSERT_EQ(stringValue.value(), "nested");

    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, MixedTypesSequence)
{
    ASSERT_TRUE(packer_->pack(true));
    ASSERT_TRUE(packer_->pack(42));
    ASSERT_TRUE(packer_->pack(3.14159));
    ASSERT_TRUE(packer_->pack("test"));
    ASSERT_TRUE(packer_->packNil());
    resetUnpacker();

    auto boolResult = unpacker_->unpackBool();
    ASSERT_TRUE(boolResult.has_value());
    ASSERT_EQ(boolResult.value(), true);

    auto intResult = unpacker_->unpackInt32();
    ASSERT_TRUE(intResult.has_value());
    ASSERT_EQ(intResult.value(), 42);

    auto doubleResult = unpacker_->unpackDouble();
    ASSERT_TRUE(doubleResult.has_value());
    ASSERT_DOUBLE_EQ(doubleResult.value(), 3.14159);

    auto stringResult = unpacker_->unpackString();
    ASSERT_TRUE(stringResult.has_value());
    ASSERT_EQ(stringResult.value(), "test");
}

TEST_F(PackerUnpackerIntegrationTest, ArrayOfStrings)
{
    std::vector<std::string> strings = {"apple", "banana", "cherry", "date"};

    ASSERT_TRUE(packer_->packArrayStart(strings.size()));
    for (const auto& str : strings)
    {
        ASSERT_TRUE(packer_->pack(nonstd::string_view(str)));
    }
    resetUnpacker();

    auto arraySize = unpacker_->unpackArraySize();
    ASSERT_TRUE(arraySize.has_value());
    ASSERT_EQ(arraySize.value(), strings.size());

    for (const auto& expected : strings)
    {
        auto str = unpacker_->unpackString();
        ASSERT_TRUE(str.has_value());
        ASSERT_EQ(str.value(), expected);
    }
    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, MapOfMixedTypes)
{
    ASSERT_TRUE(packer_->packMapStart(3));

    ASSERT_TRUE(packer_->pack("int_key"));
    ASSERT_TRUE(packer_->pack(100));

    ASSERT_TRUE(packer_->pack("bool_key"));
    ASSERT_TRUE(packer_->pack(true));

    ASSERT_TRUE(packer_->pack("string_key"));
    ASSERT_TRUE(packer_->pack("value"));
    resetUnpacker();

    auto mapSize = unpacker_->unpackMapSize();
    ASSERT_TRUE(mapSize.has_value());
    ASSERT_EQ(mapSize.value(), 3);

    auto key1 = unpacker_->unpackString();
    ASSERT_TRUE(key1.has_value());
    ASSERT_EQ(key1.value(), "int_key");
    auto val1 = unpacker_->unpackInt32();
    ASSERT_TRUE(val1.has_value());
    ASSERT_EQ(val1.value(), 100);

    auto key2 = unpacker_->unpackString();
    ASSERT_TRUE(key2.has_value());
    ASSERT_EQ(key2.value(), "bool_key");
    auto val2 = unpacker_->unpackBool();
    ASSERT_TRUE(val2.has_value());
    ASSERT_EQ(val2.value(), true);

    auto key3 = unpacker_->unpackString();
    ASSERT_TRUE(key3.has_value());
    ASSERT_EQ(key3.value(), "string_key");
    auto val3 = unpacker_->unpackString();
    ASSERT_TRUE(val3.has_value());
    ASSERT_EQ(val3.value(), "value");

    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, DeepNesting)
{
    ASSERT_TRUE(packer_->packArrayStart(1));
    ASSERT_TRUE(packer_->packArrayStart(1));
    ASSERT_TRUE(packer_->packArrayStart(1));
    ASSERT_TRUE(packer_->pack(42));
    resetUnpacker();

    for (int i = 0; i < 3; ++i)
    {
        auto size = unpacker_->unpackArraySize();
        ASSERT_TRUE(size.has_value());
        ASSERT_EQ(size.value(), 1);
    }

    auto value = unpacker_->unpackInt32();
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), 42);

    ASSERT_FALSE(unpacker_->hasMore());
}

TEST_F(PackerUnpackerIntegrationTest, MultipleReset)
{
    ASSERT_TRUE(packer_->pack(42));

    resetUnpacker();
    auto value = unpacker_->unpackInt32();
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), 42);

    packer_->reset();
    ASSERT_TRUE(packer_->pack("hello"));
    resetUnpacker();

    auto str = unpacker_->unpackString();
    ASSERT_TRUE(str.has_value());
    ASSERT_EQ(str.value(), "hello");
}

TEST_F(PackerUnpackerIntegrationTest, BoundaryValues)
{
    ASSERT_TRUE(packer_->pack(static_cast<int8_t>(INT8_MIN)));
    ASSERT_TRUE(packer_->pack(static_cast<int8_t>(INT8_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<uint8_t>(UINT8_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<int16_t>(INT16_MIN)));
    ASSERT_TRUE(packer_->pack(static_cast<int16_t>(INT16_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<uint16_t>(UINT16_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<int32_t>(INT32_MIN)));
    ASSERT_TRUE(packer_->pack(static_cast<int32_t>(INT32_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<uint32_t>(UINT32_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<int64_t>(INT64_MIN)));
    ASSERT_TRUE(packer_->pack(static_cast<int64_t>(INT64_MAX)));
    ASSERT_TRUE(packer_->pack(static_cast<uint64_t>(UINT64_MAX)));
    resetUnpacker();

    ASSERT_EQ(unpacker_->unpackInt8().value(), INT8_MIN);
    ASSERT_EQ(unpacker_->unpackInt8().value(), INT8_MAX);
    ASSERT_EQ(unpacker_->unpackUInt8().value(), UINT8_MAX);
    ASSERT_EQ(unpacker_->unpackInt16().value(), INT16_MIN);
    ASSERT_EQ(unpacker_->unpackInt16().value(), INT16_MAX);
    ASSERT_EQ(unpacker_->unpackUInt16().value(), UINT16_MAX);
    ASSERT_EQ(unpacker_->unpackInt32().value(), INT32_MIN);
    ASSERT_EQ(unpacker_->unpackInt32().value(), INT32_MAX);
    ASSERT_EQ(unpacker_->unpackUInt32().value(), UINT32_MAX);
    ASSERT_EQ(unpacker_->unpackInt64().value(), INT64_MIN);
    ASSERT_EQ(unpacker_->unpackInt64().value(), INT64_MAX);
    ASSERT_EQ(unpacker_->unpackUInt64().value(), UINT64_MAX);
}
