#include <gtest/gtest.h>
#include <casket/opt/typed_value_handler.hpp>

using namespace casket::opt;

TEST(TypedValueHandlerTest, ParseIntegerSuccess)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string> args = {"42"};

    EXPECT_NO_THROW({ intValue.parse(value, args); });

    EXPECT_EQ(std::any_cast<int>(value), 42);
    intValue.notify(value);
    EXPECT_EQ(valueStorage, 42);
}

TEST(TypedValueHandlerTest, ParseNegativeIntegerSuccess)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string> args = {"-42"};

    EXPECT_NO_THROW({ intValue.parse(value, args); });

    EXPECT_EQ(std::any_cast<int>(value), -42);
    intValue.notify(value);
    EXPECT_EQ(valueStorage, -42);
}


TEST(TypedValueHandlerTest, ParseIntegerFailure)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string> args = {"not_a_number"};

    EXPECT_THROW({ intValue.parse(value, args); }, std::runtime_error);

    EXPECT_EQ(valueStorage, 0);
}

TEST(TypedValueHandlerTest, ParseDoubleSuccess)
{
    double valueStorage = 0.0;
    TypedValueHandler<double> doubleValue(&valueStorage);

    std::any value;
    std::vector<std::string> args = {"3.14"};

    EXPECT_NO_THROW({ doubleValue.parse(value, args); });

    EXPECT_DOUBLE_EQ(std::any_cast<double>(value), 3.14);
    doubleValue.notify(value);
    EXPECT_DOUBLE_EQ(valueStorage, 3.14);
}

TEST(TypedValueHandlerTest, ParseStringSuccess)
{
    std::string valueStorage;
    TypedValueHandler<std::string> stringValue(&valueStorage);

    std::any value;
    std::vector<std::string> args = {"hello"};

    EXPECT_NO_THROW({ stringValue.parse(value, args); });

    EXPECT_EQ(std::any_cast<std::string>(value), "hello");
    stringValue.notify(value);
    EXPECT_EQ(valueStorage, "hello");
}

TEST(TypedValueHandlerTest, TokensCount)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    EXPECT_EQ(intValue.minTokens(), 1);
    EXPECT_EQ(intValue.maxTokens(), 1);
}
