#include <sstream>

#include <gtest/gtest.h>

#include <casket/opt/config_options_reader.hpp>
#include <casket/opt/config_options.hpp>
#include <casket/opt/option_builder.hpp>

using namespace casket::opt;

class TestSection : public Section
{
public:
    static std::string name()
    {
        return "TestSection";
    }
};

class OptionsReaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        config.add<TestSection>();
    }

    void TearDown() override
    {
    }

    ConfigOptions config;
    ConfigOptionsReader reader;
};

TEST_F(OptionsReaderTest, ReadSingleSection)
{
    config.get<TestSection>()->addOption(OptionBuilder("key", Value<std::string>()).build());

    std::istringstream input("TestSection {\nkey value\n}");
    ASSERT_NO_THROW(reader.read(input, config));

    auto option = config.get<TestSection>()->getOption("key");
    ASSERT_EQ(option.get<std::string>(), "value");
}

TEST_F(OptionsReaderTest, ReadMultipleSections)
{
    config.get<TestSection>()->addOption(OptionBuilder("key1", Value<std::string>()).build());
    config.get<TestSection>()->addOption(OptionBuilder("key2", Value<std::string>()).build());

    std::istringstream input("TestSection {\nkey1 value1\n}\nTestSection {\nkey2 value2\n}");
    ASSERT_NO_THROW(reader.read(input, config));

    auto option = config.get<TestSection>()->getOption("key1");
    ASSERT_EQ(option.get<std::string>(), "value1");

    option = config.get<TestSection>()->getOption("key2");
    ASSERT_EQ(option.get<std::string>(), "value2");
}

TEST_F(OptionsReaderTest, MismatchedClosingBrace)
{
    std::istringstream input("TestSection {\nkey value\n");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, UnknownSection)
{
    std::istringstream input("UnknownSection {\nkey value\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}