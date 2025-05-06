#include <gtest/gtest.h>
#include <casket/opt/option_builder.hpp>
#include <casket/opt/cmd_line_options_parser.hpp>

using namespace casket::opt;

TEST(TypedValueTest, ParseIntegerSuccess)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string_view> args = {"42"};

    EXPECT_NO_THROW({ intValue.parse(value, args); });

    EXPECT_EQ(std::any_cast<int>(value), 42);
    intValue.notify(value);
    EXPECT_EQ(valueStorage, 42);
}

TEST(TypedValueTest, ParseIntegerFailure)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string_view> args = {"not_a_number"};

    EXPECT_THROW({ intValue.parse(value, args); }, std::runtime_error);

    EXPECT_EQ(valueStorage, 0); // значение не должно измениться
}

TEST(TypedValueTest, ParseDoubleSuccess)
{
    double valueStorage = 0.0;
    TypedValueHandler<double> doubleValue(&valueStorage);

    std::any value;
    std::vector<std::string_view> args = {"3.14"};

    EXPECT_NO_THROW({ doubleValue.parse(value, args); });

    EXPECT_DOUBLE_EQ(std::any_cast<double>(value), 3.14);
    doubleValue.notify(value);
    EXPECT_DOUBLE_EQ(valueStorage, 3.14);
}

TEST(TypedValueTest, ParseStringSuccess)
{
    std::string valueStorage;
    TypedValueHandler<std::string> stringValue(&valueStorage);

    std::any value;
    std::vector<std::string_view> args = {"hello"};

    EXPECT_NO_THROW({ stringValue.parse(value, args); });

    EXPECT_EQ(std::any_cast<std::string>(value), "hello");
    stringValue.notify(value);
    EXPECT_EQ(valueStorage, "hello");
}

TEST(TypedValueTest, TokensCount)
{
    int valueStorage = 0;
    TypedValueHandler<int> intValue(&valueStorage);

    EXPECT_EQ(intValue.minTokens(), 1U);
    EXPECT_EQ(intValue.maxTokens(), 1U);
}

TEST(EmptyOptionTest, ThrowException)
{
    ASSERT_THROW(Option(""), std::runtime_error);
}

class CmdLineOptionsParserTest : public ::testing::Test
{
protected:
    CmdLineOptionsParser parser;
};

TEST_F(CmdLineOptionsParserTest, RequiredOptionNotSpecified)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo").setRequired().build()));

    ASSERT_THROW(parser.parse({}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, AttemptToGetOptionBeforeParsing)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo").setRequired().build()));

    ASSERT_THROW(parser.get("fooo"), std::logic_error);
}

TEST_F(CmdLineOptionsParserTest, NonExistentOption)
{
    ASSERT_NO_THROW(parser.add(Option("foo")));

    ASSERT_NO_THROW(parser.parse({}));

    ASSERT_THROW(parser.get("fooo"), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, NotEnoughArguments)
{
    ASSERT_NO_THROW(parser.add(Option("foo", Value<int>())));

    ASSERT_THROW(parser.parse({"--foo"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, TooManyArguments)
{
    ASSERT_NO_THROW(parser.add(Option("foo")));

    ASSERT_THROW(parser.parse({"--foo", "foo1", "foo2"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, DuplicatedOption)
{
    ASSERT_NO_THROW(parser.add(Option("foo")));

    ASSERT_THROW(parser.parse({"--foo", "--foo"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, AddOptionWithoutValue)
{
    ASSERT_NO_THROW(parser.add(Option("help")));

    ASSERT_NO_THROW(parser.parse({"--help"}););

    ASSERT_TRUE(parser.isUsed("help"));
}

TEST_F(CmdLineOptionsParserTest, AddOptionWithValue)
{
    int intValue{};

    ASSERT_NO_THROW(parser.add(Option("port", Value(&intValue))));

    ASSERT_NO_THROW(parser.parse({"--port=8080"}););

    ASSERT_EQ(parser.get<int>("port"), 8080);
}

TEST_F(CmdLineOptionsParserTest, UnknownOptionThrows)
{
    int intValue{};

    ASSERT_NO_THROW(parser.add(Option("port", Value(&intValue))));

    ASSERT_THROW(parser.parse({"--unknown=1234"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, OptionWithoutValueThrowsWhenGetting)
{
    ASSERT_NO_THROW(parser.add(Option("help")));

    ASSERT_NO_THROW(parser.parse({"--help"}));

    ASSERT_THROW(parser.get<int>("help"), std::runtime_error);
}
