#include <gtest/gtest.h>
#include <casket/opt/option_parser.hpp>

using namespace casket::opt;

TEST(TypedValueTest, ParseIntegerSuccess)
{
    int valueStorage = 0;
    detail::TypedValue<int> intValue(&valueStorage);

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
    detail::TypedValue<int> intValue(&valueStorage);

    std::any value;
    std::vector<std::string_view> args = {"not_a_number"};

    EXPECT_THROW({ intValue.parse(value, args); }, std::runtime_error);

    EXPECT_EQ(valueStorage, 0); // значение не должно измениться
}

TEST(TypedValueTest, ParseDoubleSuccess)
{
    double valueStorage = 0.0;
    detail::TypedValue<double> doubleValue(&valueStorage);

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
    detail::TypedValue<std::string> stringValue(&valueStorage);

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
    detail::TypedValue<int> intValue(&valueStorage);

    EXPECT_EQ(intValue.minTokens(), 1U);
    EXPECT_EQ(intValue.maxTokens(), 1U);
}

TEST(OptionTest, EmptyOptionName)
{
    ASSERT_THROW(Option("", "Invalid option"), std::runtime_error);
}

TEST(OptionTest, InvalidOptionAliasName)
{
    ASSERT_THROW(Option("foo,", "Invalid option"), std::runtime_error);
}

class OptionParserTest : public ::testing::Test
{
protected:
    OptionParser parser;
};

TEST_F(OptionParserTest, RequiredOptionNotSpecified)
{
    ASSERT_NO_THROW(parser.add("foo", "Foo option").required());

    ASSERT_THROW(parser.parse({}), std::runtime_error);
}

TEST_F(OptionParserTest, AttemptToGetOptionBeforeParsing)
{
    ASSERT_NO_THROW(parser.add("foo", "Foo option").required());

    ASSERT_THROW(parser.get("fooo"), std::logic_error);
}

TEST_F(OptionParserTest, NonExistentOption)
{
    ASSERT_NO_THROW(parser.add("foo", "Foo option"));

    ASSERT_NO_THROW(parser.parse({}));

    ASSERT_THROW(parser.get("fooo"), std::runtime_error);
}

TEST_F(OptionParserTest, NotEnoughArguments)
{
    ASSERT_NO_THROW(parser.add("foo", Value<int>(), "Foo option"));

    ASSERT_THROW(parser.parse({"--foo"}), std::runtime_error);
}

TEST_F(OptionParserTest, TooManyArguments)
{
    ASSERT_NO_THROW(parser.add("foo", "Foo option"));

    ASSERT_THROW(parser.parse({"--foo", "foo1", "foo2"}), std::runtime_error);
}

TEST_F(OptionParserTest, DuplicatedOption)
{
    ASSERT_NO_THROW(parser.add("foo", "Foo option"));

    ASSERT_THROW(parser.parse({"--foo", "--foo"}), std::runtime_error);
}

TEST_F(OptionParserTest, AddOptionWithoutValue)
{
    ASSERT_NO_THROW(parser.add("help", "Displays help information"));

    ASSERT_NO_THROW(parser.parse({"--help"}););

    ASSERT_TRUE(parser.isUsed("help"));
}

TEST_F(OptionParserTest, AddOptionWithValue)
{
    int intValue{};
    
    ASSERT_NO_THROW(parser.add("port", Value(&intValue), "Sets the port to connect to"));

    ASSERT_NO_THROW(parser.parse({"--port=8080"}););

    ASSERT_EQ(parser.get<int>("port"), 8080);
}

TEST_F(OptionParserTest, UnknownOptionThrows)
{
    int intValue{};
    
    ASSERT_NO_THROW(parser.add("port", Value(&intValue), "Sets the port to connect to"));

    ASSERT_THROW(parser.parse({"--unknown=1234"}), std::runtime_error);
}

TEST_F(OptionParserTest, OptionWithoutValueThrowsWhenGetting)
{
    ASSERT_NO_THROW(parser.add("help", "Displays help information"));

    ASSERT_NO_THROW(parser.parse({"--help"}));

    ASSERT_THROW(parser.get<int>("help"), std::runtime_error);
}

TEST_F(OptionParserTest, OptionWithAndWithoutAlias)
{
    ASSERT_NO_THROW(parser.add("output,o", Value<std::string>(), "Specifies the output file"));

    ASSERT_NO_THROW(parser.parse({"-o", "file.txt"}););

    ASSERT_TRUE(parser.isUsed("output"));
}
