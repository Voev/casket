#include <gtest/gtest.h>
#include <casket/opt/option_builder.hpp>
#include <casket/opt/cmd_line_options_parser.hpp>

using namespace casket::opt;

class CmdLineOptionsParserTest : public ::testing::Test
{
protected:
    CmdLineOptionsParser parser;
};

TEST_F(CmdLineOptionsParserTest, RequiredOptionNotSpecified)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo").setRequired().build()));

    ASSERT_NO_THROW(parser.parse({}));

    ASSERT_THROW(parser.validate(), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, RequiredOptionWithDefaultValue)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo").setDefaultValue(42).setRequired().build()));

    ASSERT_NO_THROW(parser.parse({}));

    ASSERT_NO_THROW(parser.validate());

    ASSERT_EQ(42, parser.get<int>("foo"));
}

TEST_F(CmdLineOptionsParserTest, AttemptToGetOptionBeforeParsing)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo").build()));

    ASSERT_THROW(parser.get("foo"), std::logic_error);
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

TEST_F(CmdLineOptionsParserTest, NotEnoughArgumentsForOptionWithDefaultValue)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo", Value<int>()).setDefaultValue("42").build()));

    ASSERT_THROW(parser.parse({"--foo"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, UseDefaultValueWhenNotProvided)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo", Value<int>()).setDefaultValue(42).build()));

    ASSERT_NO_THROW(parser.parse({}));

    ASSERT_NO_THROW(parser.validate());

    ASSERT_EQ(42, parser.get<int>("foo"));
}

TEST_F(CmdLineOptionsParserTest, GetProvidedValueInsteadOfDefaultValue)
{
    ASSERT_NO_THROW(parser.add(OptionBuilder("foo", Value<int>()).setDefaultValue(42).build()));

    ASSERT_NO_THROW(parser.parse({"--foo", "43"}));

    ASSERT_NO_THROW(parser.validate());

    ASSERT_EQ(43, parser.get<int>("foo"));
}

TEST_F(CmdLineOptionsParserTest, TooManyArguments)
{
    ASSERT_NO_THROW(parser.add(Option("foo")));

    ASSERT_THROW(parser.parse({"--foo", "foo1", "foo2"}), std::runtime_error);
}

TEST_F(CmdLineOptionsParserTest, PassOptionAsStringArgument)
{
    ASSERT_NO_THROW(parser.add(Option("foo", Value<std::string>())));

    ASSERT_THROW(parser.parse({"--foo", "--foo1"}), std::runtime_error);
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
