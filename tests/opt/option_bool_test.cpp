#include <gtest/gtest.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class BoolOptionTest : public ::testing::TestWithParam<std::pair<std::string, bool>>
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_P(BoolOptionTest, ParseBoolValues)
{
    auto [input, expected] = GetParam();

    Option opt("opt", Value<bool>());
    opt.consume(SpanBuilder::make(input));

    ASSERT_EQ(opt.get<bool>(), expected);
}

INSTANTIATE_TEST_SUITE_P(BoolValues, BoolOptionTest,
                         ::testing::Values(
                             // True values
                             std::make_pair("true", true), std::make_pair("True", true), std::make_pair("TRUE", true),
                             std::make_pair("yes", true), std::make_pair("YES", true), std::make_pair("on", true),
                             std::make_pair("ON", true), std::make_pair("1", true),

                             // False values
                             std::make_pair("false", false), std::make_pair("False", false),
                             std::make_pair("FALSE", false), std::make_pair("no", false), std::make_pair("NO", false),
                             std::make_pair("off", false), std::make_pair("OFF", false), std::make_pair("0", false)));

class InvalidBoolOptionTest : public ::testing::TestWithParam<std::string>
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_P(InvalidBoolOptionTest, ParseInvalidBoolValues)
{
    Option opt("opt", Value<bool>());
    EXPECT_THROW(opt.consume(SpanBuilder::make(GetParam())), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(InvalidBoolValues, InvalidBoolOptionTest,
                         ::testing::Values("invalid", "maybe", "2", "truefalse", "true true", "false false", "ye",
                                           " n ", ""));