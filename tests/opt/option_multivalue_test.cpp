#include <gtest/gtest.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class MultiValueIntTest : public ::testing::TestWithParam<std::pair<std::vector<std::string>, std::vector<int>>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(MultiValueIntTest, ParseMultiIntValues)
{
    auto [input, expected] = GetParam();
    
    std::vector<int> values;
    Option opt("opt", MultiValue(&values));
    
    auto span = SpanBuilder::make(input);
    opt.consume(span);
    opt.validate();
    
    ASSERT_EQ(values.size(), expected.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_EQ(values[i], expected[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MultiIntValues,
    MultiValueIntTest,
    ::testing::Values(
        std::make_pair(std::vector<std::string>{"42"}, std::vector<int>{42}),
        std::make_pair(std::vector<std::string>{"-42"}, std::vector<int>{-42}),
        std::make_pair(std::vector<std::string>{"0"}, std::vector<int>{0}),
        
        std::make_pair(std::vector<std::string>{"1", "2", "3"}, std::vector<int>{1, 2, 3}),
        std::make_pair(std::vector<std::string>{"10", "20", "30", "40"}, std::vector<int>{10, 20, 30, 40}),
        std::make_pair(std::vector<std::string>{"-1", "-2", "-3"}, std::vector<int>{-1, -2, -3}),
        std::make_pair(std::vector<std::string>{"1", "-2", "3", "-4"}, std::vector<int>{1, -2, 3, -4}),
        
        std::make_pair(std::vector<std::string>{"100", "200", "300"}, std::vector<int>{100, 200, 300})
    )
);

class MultiValueUnsignedTest : public ::testing::TestWithParam<std::pair<std::vector<std::string>, std::vector<unsigned int>>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(MultiValueUnsignedTest, ParseMultiUnsignedValues)
{
    auto [input, expected] = GetParam();
    
    std::vector<unsigned int> values;
    Option opt("opt", MultiValue(&values));
    
    opt.consume(SpanBuilder::make(input));
    opt.validate();
    
    ASSERT_EQ(values.size(), expected.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_EQ(values[i], expected[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MultiUnsignedValues,
    MultiValueUnsignedTest,
    ::testing::Values(
        std::make_pair(std::vector<std::string>{"42"}, std::vector<unsigned int>{42}),
        std::make_pair(std::vector<std::string>{"0"}, std::vector<unsigned int>{0}),
        std::make_pair(std::vector<std::string>{"1", "2", "3"}, std::vector<unsigned int>{1, 2, 3}),
        std::make_pair(std::vector<std::string>{"100", "200", "300"}, std::vector<unsigned int>{100, 200, 300}),
        std::make_pair(std::vector<std::string>{"4294967295"}, std::vector<unsigned int>{4294967295U})
    )
);

class MultiValueDoubleTest : public ::testing::TestWithParam<std::pair<std::vector<std::string>, std::vector<double>>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(MultiValueDoubleTest, ParseMultiDoubleValues)
{
    auto [input, expected] = GetParam();
    
    std::vector<double> values;
    Option opt("opt", MultiValue(&values));
    
    opt.consume(SpanBuilder::make(input));
    opt.validate();
    
    ASSERT_EQ(values.size(), expected.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_DOUBLE_EQ(values[i], expected[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MultiDoubleValues,
    MultiValueDoubleTest,
    ::testing::Values(
        std::make_pair(std::vector<std::string>{"3.14"}, std::vector<double>{3.14}),
        std::make_pair(std::vector<std::string>{"-3.14"}, std::vector<double>{-3.14}),
        std::make_pair(std::vector<std::string>{"0.0"}, std::vector<double>{0.0}),
        std::make_pair(std::vector<std::string>{"1.1", "2.2", "3.3"}, std::vector<double>{1.1, 2.2, 3.3}),
        std::make_pair(std::vector<std::string>{"1e10", "2e-5"}, std::vector<double>{1e10, 2e-5})
    )
);

class MultiValueStringTest : public ::testing::TestWithParam<std::pair<std::vector<std::string>, std::vector<std::string>>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(MultiValueStringTest, ParseMultiStringValues)
{
    auto [input, expected] = GetParam();
    
    std::vector<std::string> values;
    Option opt("opt", MultiValue(&values));
    
    opt.consume(SpanBuilder::make(input));
    opt.validate();
    
    ASSERT_EQ(values.size(), expected.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_EQ(values[i], expected[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MultiStringValues,
    MultiValueStringTest,
    ::testing::Values(
        std::make_pair(std::vector<std::string>{"hello"}, std::vector<std::string>{"hello"}),
        std::make_pair(std::vector<std::string>{"hello", "world"}, std::vector<std::string>{"hello", "world"}),
        std::make_pair(std::vector<std::string>{"a", "b", "c"}, std::vector<std::string>{"a", "b", "c"}),
        std::make_pair(std::vector<std::string>{"with spaces"}, std::vector<std::string>{"with spaces"}),
        std::make_pair(std::vector<std::string>{"", "empty"}, std::vector<std::string>{"", "empty"}),
        std::make_pair(std::vector<std::string>{"1", "2", "three", "4"}, std::vector<std::string>{"1", "2", "three", "4"})
    )
);

class MultiValueCountValidationTest : public ::testing::TestWithParam<std::tuple<size_t, size_t, std::vector<std::string>, bool>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(MultiValueCountValidationTest, ValidateMinMaxCount)
{
    auto [minCount, maxCount, input, shouldThrow] = GetParam();
    
    std::vector<int> values;
    Option opt("opt", MultiValue(&values, minCount, maxCount));
    
    if (shouldThrow)
    {
        EXPECT_THROW(opt.consume(SpanBuilder::make(input)), std::runtime_error);
    }
    else
    {
        EXPECT_NO_THROW(opt.consume(SpanBuilder::make(input)));
    }
}

INSTANTIATE_TEST_SUITE_P(
    CountValidation,
    MultiValueCountValidationTest,
    ::testing::Values(
        // (min, max, input, shouldThrow)
        std::make_tuple(3, 3, std::vector<std::string>{"1", "2", "3"}, false),
        std::make_tuple(3, 3, std::vector<std::string>{"1", "2"}, true),
        std::make_tuple(3, 3, std::vector<std::string>{"1", "2", "3", "4"}, true),
        
        std::make_tuple(2, 5, std::vector<std::string>{"1", "2"}, false),
        std::make_tuple(2, 5, std::vector<std::string>{"1", "2", "3", "4", "5"}, false),
        std::make_tuple(2, 5, std::vector<std::string>{"1"}, true),
        std::make_tuple(2, 5, std::vector<std::string>{"1", "2", "3", "4", "5", "6"}, true),
        
        std::make_tuple(0, 5, std::vector<std::string>{}, false),
        std::make_tuple(0, 5, std::vector<std::string>{"1"}, false),
        
        std::make_tuple(0, 5, std::vector<std::string>{}, false),
        std::make_tuple(1, 5, std::vector<std::string>{}, true)
    )
);

class InvalidMultiValueTest : public ::testing::TestWithParam<std::pair<std::vector<std::string>, std::string>>
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(InvalidMultiValueTest, ParseInvalidValues)
{
    auto [input, expectedError] = GetParam();
    
    std::vector<int> values;
    Option opt("opt", MultiValue(&values));
    
    EXPECT_THROW(opt.consume(SpanBuilder::make(input)), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMultiValues,
    InvalidMultiValueTest,
    ::testing::Values(
        std::make_pair(std::vector<std::string>{"not_a_number"}, "invalid integer"),
        std::make_pair(std::vector<std::string>{"42", "not_a_number", "43"}, "invalid integer"),
        std::make_pair(std::vector<std::string>{"", "42"}, "empty value"),
        std::make_pair(std::vector<std::string>{"42", "", "43"}, "empty value")
    )
);
