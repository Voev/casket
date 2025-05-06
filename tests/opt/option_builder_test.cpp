#include <gtest/gtest.h>
#include <casket/opt/option_builder.hpp>

using namespace casket::opt;

class OptionBuilderTest : public ::testing::Test {};

TEST_F(OptionBuilderTest, DefaultConstructor) {
    OptionBuilder builder("opt", Value<int>());
    auto option = builder.build();
    EXPECT_NO_THROW(option.validate());
}

TEST_F(OptionBuilderTest, ConstructorWithValue) {
    int value = 0;
    OptionBuilder builder("opt", Value(&value));
    auto option = builder.build();
    EXPECT_NO_THROW(option.validate());
}

TEST_F(OptionBuilderTest, RequiredOption) {
    OptionBuilder builder("opt", Value<int>());
    builder.setRequired();
    auto option = builder.build();
    EXPECT_THROW(option.validate(), std::runtime_error);
}

TEST_F(OptionBuilderTest, DefaultValue) {
    OptionBuilder builder("opt", Value<int>());
    builder.setDefaultValue(42);
    auto option = builder.build();
    EXPECT_NO_THROW(option.validate());
    EXPECT_EQ(option.get<int>(), 42);
}

TEST_F(OptionBuilderTest, BuildOption) {
    OptionBuilder builder("opt", Value<int>());
    auto option = builder.setRequired().setDefaultValue(42).build();
    EXPECT_NO_THROW(option.validate());
    EXPECT_EQ(option.get<int>(), 42);
}
