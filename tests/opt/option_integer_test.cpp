#include <gtest/gtest.h>
#include <limits.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class IntOptionTest : public ::testing::TestWithParam<std::pair<std::string, int>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(IntOptionTest, ParseIntValues)
{
    auto [input, expected] = GetParam();
    
    Option opt("opt", Value<int>());
    opt.consume(SpanBuilder::make(input));
    
    ASSERT_EQ(opt.get<int>(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    IntValues,
    IntOptionTest,
    ::testing::Values(
        std::make_pair("0", 0),
        std::make_pair("42", 42),
        std::make_pair("-42", -42),
        std::make_pair("2147483647", INT_MAX),
        std::make_pair("-2147483648", INT_MIN)
    )
);

class InvalidIntOptionTest : public ::testing::TestWithParam<std::string>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(InvalidIntOptionTest, ParseInvalidIntValues)
{
    Option opt("opt", Value<int>());
    EXPECT_THROW(opt.consume(SpanBuilder::make(GetParam())), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidIntValues,
    InvalidIntOptionTest,
    ::testing::Values(
        "",
        "not_a_number",
        "42.5",
        "42 43",
        "2147483648",  // INT_MAX + 1
        "-2147483649"   // INT_MIN - 1
    )
);