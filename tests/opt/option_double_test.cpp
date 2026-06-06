#include <gtest/gtest.h>
#include <limits.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class DoubleOptionTest : public ::testing::TestWithParam<std::pair<std::string, double>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(DoubleOptionTest, ParseDoubleValues)
{
    auto [input, expected] = GetParam();
    
    Option opt("opt", Value<double>());
    opt.consume(SpanBuilder::make(input));
    
    ASSERT_DOUBLE_EQ(opt.get<double>(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    DoubleValues,
    DoubleOptionTest,
    ::testing::Values(
        std::make_pair("0.0", 0.0),
        std::make_pair("3.14", 3.14),
        std::make_pair("-3.14", -3.14),
        std::make_pair("1.0e10", 1.0e10),
        std::make_pair("1e-10", 1e-10)
    )
);