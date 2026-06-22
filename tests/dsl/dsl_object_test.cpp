#include <gtest/gtest.h>
#include <casket/dsl/dsl.hpp>

using namespace casket::dsl;

class DslObjectTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(DslObjectTest, ObjectGetExistingKey)
{
    auto obj = std::make_unique<Object>();
    obj->fields.at("name") = Value(std::string("Alex"));
    obj->fields.at("age") = Value(Integer(30));

    auto name = obj->get<std::string>("name");
    EXPECT_TRUE(name.has_value());
    EXPECT_EQ(*name, "Alex");

    auto age = obj->get<Integer>("age");
    EXPECT_TRUE(age.has_value());
    EXPECT_EQ(*age, 30);
}

TEST_F(DslObjectTest, ObjectGetMissingKey)
{
    auto obj = std::make_unique<Object>();
    auto value = obj->get<std::string>("missing");
    EXPECT_FALSE(value.has_value());
}

TEST_F(DslObjectTest, ObjectGetTypeMismatch)
{
    auto obj = std::make_unique<Object>();
    obj->fields.at("value") = Value(std::string("text"));

    auto intVal = obj->get<Integer>("value");
    EXPECT_FALSE(intVal.has_value());
}

TEST_F(DslObjectTest, ObjectGetOr)
{
    auto obj = std::make_unique<Object>();
    obj->fields.at("name") = Value(std::string("Alex"));

    EXPECT_EQ(obj->getOr<std::string>("name", "default"), "Alex");
    EXPECT_EQ(obj->getOr<std::string>("missing", "default"), "default");
    EXPECT_EQ(obj->getOr<Integer>("missing", 42), 42);
}
