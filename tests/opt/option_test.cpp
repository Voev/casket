#include <gtest/gtest.h>
#include <casket/opt/option.hpp>

using namespace casket::opt;

class OptionTest : public ::testing::Test
{
};

TEST_F(OptionTest, DefaultConstructor)
{
    Option opt("opt");
    EXPECT_NO_THROW(opt.validate());
}

TEST_F(OptionTest, ConstructorWithValueSemantic)
{
    auto valueSemantic = std::make_shared<UntypedValueHandler>();
    Option opt("opt", valueSemantic);
    EXPECT_NO_THROW(opt.validate());
}

TEST_F(OptionTest, ConsumeOption)
{
    Option opt("opt", Value<int>());
    std::vector<std::string> args = {"42"};
    opt.consume(args.begin(), args.end());
    EXPECT_EQ(opt.get<int>(), 42);
}

TEST_F(OptionTest, ConsumeOptionWithTooFewArguments)
{
    Option opt("opt", Value<int>());
    std::vector<std::string> args = {};
    EXPECT_THROW(opt.consume(args.begin(), args.end()), std::runtime_error);
}

TEST_F(OptionTest, ConsumeOptionWithTooManyArguments)
{
    Option opt("opt", Value<int>());
    std::vector<std::string> args = {"42", "43"};
    EXPECT_THROW(opt.consume(args.begin(), args.end()), std::runtime_error);
}

TEST_F(OptionTest, GetOption)
{
    Option opt("opt", Value<int>());
    std::vector<std::string> args = {"42"};
    opt.consume(args.begin(), args.end());
    EXPECT_EQ(opt.get<int>(), 42);
}

TEST_F(OptionTest, PresentOption)
{
    Option opt("opt", Value<int>());
    std::vector<std::string> args = {"42"};
    opt.consume(args.begin(), args.end());
    EXPECT_TRUE(opt.present<int>().has_value());
    EXPECT_EQ(opt.present<int>().value(), 42);
}
