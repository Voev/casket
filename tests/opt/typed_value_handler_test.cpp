#include <gtest/gtest.h>
#include <casket/opt/typed_value_handler.hpp>

using namespace casket::opt;

TEST(TypedValueHandlerTest, TokensCount)
{
    auto handler = Value<int>();
    ASSERT_NE(handler, nullptr);

    EXPECT_EQ(handler->minTokens(), 1);
    EXPECT_EQ(handler->maxTokens(), 1);
}

TEST(TypedValueHandlerTest, NoStorageForValue)
{
    auto handler = Value<std::string>();
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {"hello"};

    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));

    ASSERT_EQ(std::any_cast<std::string>(anyValue), args.front());
}

TEST(TypedValueHandlerTest, UseStorageForValue)
{
    std::string value = "hello";

    auto handler = Value(&value);
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {value};

    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_EQ(value, args.front());
}

TEST(TypedValueHandlerTest, ParseIntegerSuccess)
{
    int value = 0;
    auto handler = Value(&value);

    std::vector<std::string> args = {"42"};

    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_EQ(value, 42);
}

TEST(TypedValueHandlerTest, ParseNegativeIntegerSuccess)
{
    int value = 0;

    auto handler = Value(&value);
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {"-42"};

    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_EQ(value, -42);
}

TEST(TypedValueHandlerTest, ParseIntegerFailure)
{
    int value = 0;

    auto handler = Value(&value);
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {"not_a_number"};

    std::any anyValue;
    ASSERT_THROW(handler->parse(anyValue, args), std::runtime_error);
}

TEST(TypedValueHandlerTest, ParseDoubleSuccess)
{
    double value = 0.0;

    auto handler = Value(&value);
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {"3.14"};

    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_DOUBLE_EQ(value, 3.14);
}

TEST(TypedValueHandlerTest, ParseStringSuccess)
{
    std::string value;
    auto handler = Value(&value);
    ASSERT_NE(handler, nullptr);

    std::vector<std::string> args = {"hello"};
    
    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_EQ(value, "hello");
}
