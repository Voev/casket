#include <gtest/gtest.h>
#include <casket/opt/multi_value_handler.hpp>

using namespace casket::opt;

TEST(MultiValueHandlerTest, ParseSingleValue)
{
    std::vector<std::string> args = {"42"};
    
    std::vector<int> values;
    auto handler = MultiValue(&values);
    
    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    ASSERT_EQ(values, std::vector{42});
}

TEST(MultiValueHandlerTest, ParseMultiValue)
{
    std::vector<std::string> args = {"42", "-43", "44"};
    
    std::vector<int> values;
    auto handler = MultiValue(&values);
    
    std::any anyValue;
    ASSERT_NO_THROW(handler->parse(anyValue, args));
    ASSERT_NO_THROW(handler->notify(anyValue));

    auto expected = std::vector<int>{42, -43, 44};
    ASSERT_EQ(values, expected);
}
