#include <gtest/gtest.h>
#include <casket/dsl/dsl.hpp>
#include <chrono>

using namespace casket::dsl;

class DslTest : public ::testing::Test
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

TEST_F(DslTest, IntegrationGenKeySimple)
{
    std::string input = "type=rsa size=4096 output=key.pem";
    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(getString(root->fields.at("type")), "rsa");
    EXPECT_EQ(getInteger(root->fields.at("size")), 4096);
    EXPECT_EQ(getString(root->fields.at("output")), "key.pem");
}

TEST_F(DslTest, IntegrationGenKeyWithOptions)
{
    std::string input = R"(
        type=rsa
        size=2048
        output=secure.key
        options {
            encrypt=true
            format=pkcs8
            iterations=50000
            cipher=aes256
            password=secret123
        }
    )";

    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    auto* options = getObject(root->fields.at("options"));
    ASSERT_NE(options, nullptr);

    EXPECT_TRUE(getBool(options->fields.at("encrypt")));
    EXPECT_EQ(getString(options->fields.at("format")), "pkcs8");
    EXPECT_EQ(getInteger(options->fields.at("iterations")), 50000);
    EXPECT_EQ(getString(options->fields.at("cipher")), "aes256");
    EXPECT_EQ(getString(options->fields.at("password")), "secret123");
}

TEST_F(DslTest, IntegrationGenKeyWithMetadata)
{
    std::string input = R"(
        type=ec
        size=256
        output=ec-key.pem
        metadata {
            author {
                name="John Doe"
                email=john@example.com
            }
            environment=prod
            version=2
        }
    )";

    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    auto* metadata = getObject(root->fields.at("metadata"));
    ASSERT_NE(metadata, nullptr);

    auto* author = getObject(metadata->fields.at("author"));
    ASSERT_NE(author, nullptr);

    EXPECT_EQ(getString(author->fields.at("name")), "John Doe");
    EXPECT_EQ(getString(author->fields.at("email")), "john@example.com");
    EXPECT_EQ(getString(metadata->fields.at("environment")), "prod");
    EXPECT_EQ(getInteger(metadata->fields.at("version")), 2);
}

TEST_F(DslTest, IntegrationFullGenKey)
{
    std::string input = R"(
        type=rsa
        size=4096
        output=keys/api-key.pem
        name=api-key
        options {
            encrypt=true
            format=pkcs8
            iterations=100000
            cipher=aes256
            password=super_secret
        }
        metadata {
            author {
                name="John Doe"
                email=john@example.com
            }
            environment=prod
            version=3
            purpose=signing
        }
        tags=[api auth production]
    )";

    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    // Root
    EXPECT_EQ(getString(root->fields.at("type")), "rsa");
    EXPECT_EQ(getInteger(root->fields.at("size")), 4096);
    EXPECT_EQ(getString(root->fields.at("output")), "keys/api-key.pem");
    EXPECT_EQ(getString(root->fields.at("name")), "api-key");

    // Options
    auto* options = getObject(root->fields.at("options"));
    ASSERT_NE(options, nullptr);
    EXPECT_TRUE(getBool(options->fields.at("encrypt")));
    EXPECT_EQ(getString(options->fields.at("format")), "pkcs8");
    EXPECT_EQ(getInteger(options->fields.at("iterations")), 100000);
    EXPECT_EQ(getString(options->fields.at("cipher")), "aes256");
    EXPECT_EQ(getString(options->fields.at("password")), "super_secret");

    // Metadata
    auto* metadata = getObject(root->fields.at("metadata"));
    ASSERT_NE(metadata, nullptr);

    auto* author = getObject(metadata->fields.at("author"));
    ASSERT_NE(author, nullptr);
    EXPECT_EQ(getString(author->fields.at("name")), "John Doe");
    EXPECT_EQ(getString(author->fields.at("email")), "john@example.com");
    EXPECT_EQ(getString(metadata->fields.at("environment")), "prod");
    EXPECT_EQ(getInteger(metadata->fields.at("version")), 3);
    EXPECT_EQ(getString(metadata->fields.at("purpose")), "signing");

    // Tags
    const auto* tags = getArray(root->fields.at("tags"));
    ASSERT_NE(tags, nullptr);
    ASSERT_EQ(tags->size(), 3);
    EXPECT_EQ(getString((*tags)[0]), "api");
    EXPECT_EQ(getString((*tags)[1]), "auth");
    EXPECT_EQ(getString((*tags)[2]), "production");
}

TEST_F(DslTest, ValueIsNull)
{
    Value v1;
    EXPECT_TRUE(v1.isNull());

    Value v2(Null{});
    EXPECT_TRUE(v2.isNull());

    Value v3("hello");
    EXPECT_FALSE(v3.isNull());
}

TEST_F(DslTest, ValueTypeName)
{
    EXPECT_EQ(Value("test").typeName(), "string");
    EXPECT_EQ(Value(Integer(42)).typeName(), "integer");
    EXPECT_EQ(Value(Float(3.14)).typeName(), "float");
    EXPECT_EQ(Value(Boolean(true)).typeName(), "boolean");
    EXPECT_EQ(Value(Null{}).typeName(), "null");
    EXPECT_EQ(Value(Array{}).typeName(), "array");
    EXPECT_EQ(Value(Object{}).typeName(), "object");
}

TEST_F(DslTest, ValueToString)
{
    EXPECT_EQ(Value("test").toString(), "\"test\"");
    EXPECT_EQ(Value(Integer(42)).toString(), "42");
    EXPECT_EQ(Value(Float(3.14)).toString(), "3.14");
    EXPECT_EQ(Value(Boolean(true)).toString(), "true");
    EXPECT_EQ(Value(Null{}).toString(), "null");
}

TEST_F(DslTest, ValueAs)
{
    Value v = "hello";
    auto str = v.as<std::string>();
    EXPECT_TRUE(str.has_value());
    EXPECT_EQ(*str, "hello");

    auto intVal = v.as<Integer>();
    EXPECT_FALSE(intVal.has_value());
}
