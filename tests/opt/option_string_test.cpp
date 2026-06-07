#include <gtest/gtest.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class StringOptionTest : public ::testing::TestWithParam<std::pair<std::string, std::string>>
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_P(StringOptionTest, ParseStringValues)
{
    auto [input, expected] = GetParam();

    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make(input));

    ASSERT_EQ(opt.get<std::string>(), expected);
}

INSTANTIATE_TEST_SUITE_P(StringValues, StringOptionTest,
                         ::testing::Values(std::make_pair("hello", "hello"),
                                           std::make_pair("hello world", "hello world"), std::make_pair("", ""),
                                           std::make_pair("  spaces  ", "  spaces  "), std::make_pair("123", "123"),
                                           std::make_pair("true", "true"), std::make_pair("false", "false")));


class EscapedStringOptionTest : public ::testing::TestWithParam<std::pair<std::string, std::string>>
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_P(EscapedStringOptionTest, ParseEscapedStringValues)
{
    auto [input, expected] = GetParam();

    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make(input));

    ASSERT_EQ(opt.get<std::string>(), expected);
}

INSTANTIATE_TEST_SUITE_P(EscapedStringValues, EscapedStringOptionTest,
                         ::testing::Values(std::make_pair("\"hello\\nworld\"", "\"hello\\nworld\""),
                                           std::make_pair("\"hello\\tworld\"", "\"hello\\tworld\""),
                                           std::make_pair("\"hello\\rworld\"", "\"hello\\rworld\""),
                                           std::make_pair("\"hello\\\\world\"", "\"hello\\\\world\"")));

TEST(StringOptionTest, MultipleTokensJoined)
{
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make({"hello", "world", "from", "test"}));

    ASSERT_EQ(opt.get<std::string>(), "hello world from test");
}

TEST(StringOptionTest, MultipleTokensWithQuotes)
{
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make({"\"hello", "world\"", "test"}));

    ASSERT_EQ(opt.get<std::string>(), "\"hello world\" test");
}

TEST(StringOptionTest, SingleTokenWithSpaces)
{
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make("hello world"));

    ASSERT_EQ(opt.get<std::string>(), "hello world");
}

TEST(StringOptionTest, EmptyValue)
{
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make(""));

    ASSERT_EQ(opt.get<std::string>(), "");
}

TEST(StringOptionTest, EmptySpan)
{
    Option opt("opt", Value<std::string>());
    std::vector<nonstd::string_view> empty;
    opt.consume(nonstd::span<const nonstd::string_view>(empty));

    ASSERT_EQ(opt.get<std::string>(), "");
}

TEST(StringOptionTest, WhitespaceOnly)
{
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make("   "));

    ASSERT_EQ(opt.get<std::string>(), "   ");
}

TEST(StringOptionTest, BoundVariable)
{
    std::string value;
    Option opt("opt", Value(&value));

    opt.consume(SpanBuilder::make("test value"));
    opt.validate();

    ASSERT_EQ(value, "test value");
    ASSERT_EQ(opt.get<std::string>(), "test value");
}

TEST(StringOptionTest, VeryLongString)
{
    std::string longString(10000, 'a');
    Option opt("opt", Value<std::string>());

    opt.consume(SpanBuilder::make(longString));

    ASSERT_EQ(opt.get<std::string>(), longString);
    ASSERT_EQ(opt.get<std::string>().size(), 10000);
}

TEST(StringOptionTest, VeryLongStrаingWithSpaces)
{
    std::string longString;
    for (int i = 0; i < 1000; ++i)
    {
        longString += "word ";
    }

    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make(longString));

    ASSERT_EQ(opt.get<std::string>(), longString);
}

TEST(StringOptionTest, SpecialCharactersAsString)
{
    Option opt("opt", Value<std::string>());

    std::string withNull = "hello";
    withNull += '\0';
    withNull += "world";
    opt.consume(SpanBuilder::make(withNull));
    ASSERT_EQ(opt.get<std::string>(), withNull);
}

TEST(StringOptionTest, IsUsedFlag)
{
    Option opt("opt", Value<std::string>());

    EXPECT_FALSE(opt.isUsed());

    opt.consume(SpanBuilder::make("value"));
    EXPECT_TRUE(opt.isUsed());
}

TEST(StringOptionTest, MinMaxTokens)
{
    auto handler = Value<std::string>();
    EXPECT_EQ(handler->minTokens(), 0);
    EXPECT_EQ(handler->maxTokens(), std::numeric_limits<std::size_t>::max());
}