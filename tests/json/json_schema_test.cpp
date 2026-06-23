#include <gtest/gtest.h>
#include <casket/json/json.hpp>

using namespace casket::json;

class JsonSchemaTest : public ::testing::Test
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

TEST_F(JsonSchemaTest, SchemaValidationSuccess)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("name", "Name", true));
    schema.add(TypedParamSpec<Integer>("age", "Age", false, 18));
    schema.add(TypedParamSpec<Boolean>("active", "Active", false, true));

    Value parsed = parseDSL("{ name: \"Alex\", age: 30, active: true }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));
    EXPECT_TRUE(errors.empty());
}

TEST_F(JsonSchemaTest, SchemaValidationMissingRequired)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("name", "Name", true));
    schema.add(TypedParamSpec<Integer>("age", "Age", false, 18));

    Value parsed = parseDSL("{ age: 30 }");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));
    ASSERT_GE(errors.size(), 1);
    EXPECT_NE(errors[0].find("name"), std::string::npos);
}

TEST_F(JsonSchemaTest, SchemaValidationTypeMismatch)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("age", "Age", false));

    // age should be integer, but we pass string
    Value parsed = parseDSL("{ age: \"not_a_number\" }");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));
    ASSERT_GE(errors.size(), 1);
    EXPECT_NE(errors[0].find("Expected"), std::string::npos);
}

TEST_F(JsonSchemaTest, SchemaValidationAllowedValues)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("type", "Type", true).withAllowedValues({"rsa", "ec", "ed25519"}));

    // Valid
    Value parsed1 = parseDSL("{ type: \"rsa\" }");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid
    Value parsed2 = parseDSL("{ type: \"x509\" }");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
    ASSERT_GE(errors2.size(), 1);
    EXPECT_EQ(errors2[0], "Validation failed for 'type'");
}

TEST_F(JsonSchemaTest, SchemaValidationRange)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("size", "Size", false, 2048).withRange<Integer>(256, 8192));

    // Valid
    Value parsed1 = parseDSL("{ size: 4096 }");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid (too small)
    Value parsed2 = parseDSL("{ size: 128 }");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
    ASSERT_GE(errors2.size(), 1);
    EXPECT_NE(errors2[0].find("Validation failed"), std::string::npos);
}

TEST_F(JsonSchemaTest, SchemaValidationNestedPath)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("options.format", "Format", false, "pem"));

    Value parsed = parseDSL("{ options: { format: \"der\" } }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    // Verify default was not applied because value exists
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    ASSERT_TRUE(root->has("options"));
    auto* options = getObject(root->fields.at("options"));
    ASSERT_NE(options, nullptr);
    EXPECT_EQ(getString(options->fields.at("format")), "der");
}

TEST_F(JsonSchemaTest, SchemaValidationDefaultApplied)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("size", "Size", false, 2048));

    Value parsed = parseDSL("{ name: \"test\" }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    // Default should be applied
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    ASSERT_TRUE(root->has("size"));
    EXPECT_EQ(getInteger(root->fields.at("size")), 2048);

    // Check applied defaults tracking
    auto defaults = schema.getAppliedDefaults();
    EXPECT_EQ(defaults.size(), 1);
    EXPECT_EQ(getInteger(defaults.at("size")), 2048);
}

TEST_F(JsonSchemaTest, SchemaValidationDeepNestedPath)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("metadata.author.name", "Author name", true));

    Value parsed = parseDSL("{ metadata: { author: { name: \"John Doe\" } } }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    auto* metadata = getObject(root->fields.at("metadata"));
    ASSERT_NE(metadata, nullptr);
    auto* author = getObject(metadata->fields.at("author"));
    ASSERT_NE(author, nullptr);
    EXPECT_EQ(getString(author->fields.at("name")), "John Doe");
}

TEST_F(JsonSchemaTest, SchemaValidationMissingDeepNested)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("metadata.author.name", "Author name", true));

    Value parsed = parseDSL("{ metadata: {} }");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));
    ASSERT_GE(errors.size(), 1);
    EXPECT_NE(errors[0].find("metadata.author.name"), std::string::npos);
}

TEST_F(JsonSchemaTest, SchemaValidationWithArray)
{
    Schema schema;
    schema.add(TypedParamSpec<Array>("tags", "Tags", false));

    Value parsed = parseDSL("{ tags: [\"dev\", \"ops\", \"prod\"] }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    auto* tags = getArray(root->fields.at("tags"));
    ASSERT_NE(tags, nullptr);
    EXPECT_EQ(tags->size(), 3);
}

TEST_F(JsonSchemaTest, SchemaValidationWithObject)
{
    Schema schema;
    schema.add(TypedParamSpec<Object>("config", "Config", false));

    Value parsed = parseDSL("{ config: { host: \"localhost\", port: 5432 } }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    auto* config = getObject(root->fields.at("config"));
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(getString(config->fields.at("host")), "localhost");
    EXPECT_EQ(getInteger(config->fields.at("port")), 5432);
}

TEST_F(JsonSchemaTest, SchemaValidationMin)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("port", "Port", false, 8080).withMin(1024));

    // Valid
    Value parsed1 = parseDSL("{ port: 8080 }");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid
    Value parsed2 = parseDSL("{ port: 80 }");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
}

TEST_F(JsonSchemaTest, SchemaValidationMax)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("port", "Port", false, 8080).withMax(1024));

    // Valid
    Value parsed1 = parseDSL("{ port: 443 }");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid
    Value parsed2 = parseDSL("{ port: 8080 }");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
}

TEST_F(JsonSchemaTest, SchemaValidationMultipleErrors)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("name", "Name", true));
    schema.add(TypedParamSpec<Integer>("age", "Age", false).withRange(0, 150));
    schema.add(TypedParamSpec<std::string>("email", "Email", true));

    Value parsed = parseDSL("{ age: -10 }");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));

    // Should have at least 2 errors: missing name, missing email, invalid age
    EXPECT_GE(errors.size(), 2);
}

TEST_F(JsonSchemaTest, SchemaValidationHelpGeneration)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("type", "Key type", true).withAllowedValues({"rsa", "ec"}));
    schema.add(TypedParamSpec<Integer>("size", "Key size", false, 2048).withRange(1024, 4096));
    schema.add(TypedParamSpec<bool>("encrypt", "Encrypt the key", false, false));
    schema.add(TypedParamSpec<std::string>("output", "Output path", true));

    std::string help = schema.generateHelp();
    ASSERT_FALSE(help.empty());
}