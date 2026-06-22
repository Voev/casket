#include <gtest/gtest.h>
#include <casket/dsl/dsl.hpp>

using namespace casket::dsl;

class DslParserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    std::string getString(const Value& v)
    {
        return *v.get<std::string>();
    }

    Integer getInteger(const Value& v)
    {
        return *v.get<Integer>();
    }

    Float getDouble(const Value& v)
    {
        return *v.get<Float>();
    }

    bool getBool(const Value& v)
    {
        return *v.get<Boolean>();
    }

    const Object* getObject(const Value& v)
    {
        return v.get<Object>();
    }

    const Array* getArray(const Value& v)
    {
        return v.get<Array>();
    }
};

TEST_F(DslParserTest, ParseString)
{
    Value result = parseDSL("hello");
    EXPECT_TRUE(result.is<std::string>());
    EXPECT_EQ(getString(result), "hello");
}

TEST_F(DslParserTest, ParseInteger)
{
    Value result = parseDSL("123");
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(getInteger(result), 123);
}

TEST_F(DslParserTest, ParseNegativeInteger)
{
    Value result = parseDSL("-456");
    EXPECT_TRUE(result.is<Integer>());
    EXPECT_EQ(getInteger(result), -456);
}

TEST_F(DslParserTest, ParseFloat)
{
    Value result = parseDSL("3.14");
    EXPECT_TRUE(result.is<Float>());
    EXPECT_DOUBLE_EQ(getDouble(result), 3.14);
}

TEST_F(DslParserTest, ParseScientificNotation)
{
    Value result = parseDSL("1e-5");
    EXPECT_TRUE(result.is<Float>());
    EXPECT_DOUBLE_EQ(getDouble(result), 1e-5);
}

TEST_F(DslParserTest, ParseBooleanTrue)
{
    Value result = parseDSL("true");
    EXPECT_TRUE(result.is<Boolean>());
    EXPECT_TRUE(getBool(result));
}

TEST_F(DslParserTest, ParseBooleanFalse)
{
    Value result = parseDSL("false");
    EXPECT_TRUE(result.is<Boolean>());
    EXPECT_FALSE(getBool(result));
}

TEST_F(DslParserTest, ParseNull)
{
    Value result = parseDSL("null");
    EXPECT_TRUE(result.isNull());
}

TEST_F(DslParserTest, ParseSimpleObject)
{
    Value result = parseDSL("user { name=Alex age=30 }");
    EXPECT_TRUE(result.is<Object>());

    auto* obj = getObject(result);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->fields.size(), 1);

    auto* user = getObject(obj->fields.at("user"));
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->fields.size(), 2);

    EXPECT_EQ(getString(user->fields.at("name")), "Alex");
    EXPECT_EQ(getInteger(user->fields.at("age")), 30);
}

TEST_F(DslParserTest, ParseNestedObject)
{
    Value result = parseDSL("app { name=myapp db { host=localhost port=5432 } }");
    EXPECT_TRUE(result.is<Object>());

    auto* root = getObject(result);
    ASSERT_NE(root, nullptr);

    auto* app = getObject(root->fields.at("app"));
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(getString(app->fields.at("name")), "myapp");

    auto* db = getObject(app->fields.at("db"));
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(getString(db->fields.at("host")), "localhost");
    EXPECT_EQ(getInteger(db->fields.at("port")), 5432);
}

TEST_F(DslParserTest, ParseArray)
{
    Value result = parseDSL("[1 2 3]");
    EXPECT_TRUE(result.is<Array>());

    const auto* arr = getArray(result);
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->size(), 3);
    EXPECT_EQ(getInteger((*arr)[0]), 1);
    EXPECT_EQ(getInteger((*arr)[1]), 2);
    EXPECT_EQ(getInteger((*arr)[2]), 3);
}

TEST_F(DslParserTest, ParseMixedArray)
{
    Value result = parseDSL("[42 3.14 true \"text\"]");
    const auto* arr = getArray(result);
    ASSERT_NE(arr, nullptr);

    ASSERT_EQ(arr->size(), 4);
    EXPECT_EQ(getInteger((*arr)[0]), 42);
    EXPECT_DOUBLE_EQ(getDouble((*arr)[1]), 3.14);
    EXPECT_TRUE(getBool((*arr)[2]));
    EXPECT_EQ(getString((*arr)[3]), "text");
}

TEST_F(DslParserTest, ParseArrayOfObjects)
{
    Value result = parseDSL("[{ name=Alex age=30 } { name=Bob age=25 }]");
    const auto* arr = getArray(result);
    ASSERT_NE(arr, nullptr);

    ASSERT_EQ(arr->size(), 2);

    auto* obj1 = getObject((*arr)[0]);
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(getString(obj1->fields.at("name")), "Alex");
    EXPECT_EQ(getInteger(obj1->fields.at("age")), 30);

    auto* obj2 = getObject((*arr)[1]);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(getString(obj2->fields.at("name")), "Bob");
    EXPECT_EQ(getInteger(obj2->fields.at("age")), 25);
}

TEST_F(DslParserTest, ParseComplexNested)
{
    std::string input = R"(
        config {
            name=myapp
            port=8080
            debug=true
            features=[auth logging metrics]
            db {
                host=localhost
                port=5432
                credentials {
                    user=admin
                    password=secret
                }
            }
        }
    )";

    Value result = parseDSL(input);
    auto* root = getObject(result);
    ASSERT_NE(root, nullptr);

    auto* config = getObject(root->fields.at("config"));
    ASSERT_NE(config, nullptr);

    EXPECT_EQ(getString(config->fields.at("name")), "myapp");
    EXPECT_EQ(getInteger(config->fields.at("port")), 8080);
    EXPECT_TRUE(getBool(config->fields.at("debug")));

    const auto* features = getArray(config->fields.at("features"));
    ASSERT_NE(features, nullptr);
    ASSERT_EQ(features->size(), 3);
    EXPECT_EQ(getString((*features)[0]), "auth");
    EXPECT_EQ(getString((*features)[1]), "logging");
    EXPECT_EQ(getString((*features)[2]), "metrics");

    auto* db = getObject(config->fields.at("db"));
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(getString(db->fields.at("host")), "localhost");
    EXPECT_EQ(getInteger(db->fields.at("port")), 5432);

    auto* creds = getObject(db->fields.at("credentials"));
    ASSERT_NE(creds, nullptr);
    EXPECT_EQ(getString(creds->fields.at("user")), "admin");
    EXPECT_EQ(getString(creds->fields.at("password")), "secret");
}

TEST_F(DslParserTest, ParseErrorUnknownCharacter)
{
    EXPECT_THROW(parseDSL("key=value$"), std::runtime_error);
}

TEST_F(DslParserTest, ParseErrorUnclosedBrace)
{
    EXPECT_THROW(parseDSL("user { name=Alex "), std::runtime_error);
}

TEST_F(DslParserTest, ParseErrorUnclosedArray)
{
    EXPECT_THROW(parseDSL("[1 2 3"), std::runtime_error);
}

TEST_F(DslParserTest, ParseErrorUnclosedQuote)
{
    EXPECT_THROW(parseDSL("text=\"hello"), std::runtime_error);
}

TEST_F(DslParserTest, ParseErrorExtraTokens)
{
    EXPECT_THROW(parseDSL("key=value extra"), std::runtime_error);
}
