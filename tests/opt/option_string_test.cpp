#include <gtest/gtest.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;


class StringOptionTest : public ::testing::TestWithParam<std::pair<std::string, std::string>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(StringOptionTest, ParseStringValues)
{
    auto [input, expected] = GetParam();
    
    Option opt("opt", Value<std::string>());
    opt.consume(SpanBuilder::make(input));
    
    ASSERT_EQ(opt.get<std::string>(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    StringValues,
    StringOptionTest,
    ::testing::Values(
        std::make_pair("hello", "hello"),
        std::make_pair("hello world", "hello world"),
        std::make_pair("", ""),
        std::make_pair("  spaces  ", "  spaces  "),
        std::make_pair("123", "123"),
        std::make_pair("true", "true"),
        std::make_pair("false", "false")
    )
);