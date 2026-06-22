#include <gtest/gtest.h>
#include <casket/dsl/dsl.hpp>
#include <chrono>

using namespace casket::dsl;

class DslSchemaTest : public ::testing::Test
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

TEST_F(DslSchemaTest, SchemaValidationSuccess)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("name", "Name", true));
    schema.add(TypedParamSpec<Integer>("age", "Age", false, 18));
    schema.add(TypedParamSpec<Boolean>("active", "Active", false, true));

    Value parsed = parseDSL("name=Alex age=30");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));
    EXPECT_TRUE(errors.empty());
}

TEST_F(DslSchemaTest, SchemaValidationMissingRequired)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("name", "Name", true));
    schema.add(TypedParamSpec<Integer>("age", "Age", false, 18));

    Value parsed = parseDSL("age=30");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));
    ASSERT_GE(errors.size(), 1);
    EXPECT_NE(errors[0].find("name"), std::string::npos);
}

TEST_F(DslSchemaTest, SchemaValidationTypeMismatch)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("age", "Age", false));

    Value parsed = parseDSL("age=not_a_number");
    std::vector<std::string> errors;
    EXPECT_FALSE(schema.validate(parsed, errors));
    ASSERT_GE(errors.size(), 1);
}

TEST_F(DslSchemaTest, SchemaValidationAllowedValues)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("type", "Type", true).withAllowedValues({"rsa", "ec", "ed25519"}));

    // Valid
    Value parsed1 = parseDSL("type=rsa");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid
    Value parsed2 = parseDSL("type=x509");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
}

TEST_F(DslSchemaTest, SchemaValidationRange)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("size", "Size", false, 2048).withRange<Integer>(256, 8192));

    // Valid
    Value parsed1 = parseDSL("size=4096");
    std::vector<std::string> errors1;
    EXPECT_TRUE(schema.validate(parsed1, errors1));

    // Invalid (too small)
    Value parsed2 = parseDSL("size=128");
    std::vector<std::string> errors2;
    EXPECT_FALSE(schema.validate(parsed2, errors2));
}

TEST_F(DslSchemaTest, SchemaValidationNestedPath)
{
    Schema schema;
    schema.add(TypedParamSpec<std::string>("options.format", "Format", false, "pem"));

    Value parsed = parseDSL("options { format=der }");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    // Verify default was not applied because value exists
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    auto* options = getObject(root->fields.at("options"));
    ASSERT_NE(options, nullptr);
    EXPECT_EQ(getString(options->fields.at("format")), "der");
}

TEST_F(DslSchemaTest, SchemaValidationDefaultApplied)
{
    Schema schema;
    schema.add(TypedParamSpec<Integer>("size", "Size", false, 2048));

    Value parsed = parseDSL("name=test");
    std::vector<std::string> errors;
    EXPECT_TRUE(schema.validate(parsed, errors));

    // Default should be applied
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(getInteger(root->fields.at("size")), 2048);

    // Check applied defaults tracking
    auto defaults = schema.getAppliedDefaults();
    EXPECT_EQ(defaults.size(), 1);
    EXPECT_EQ(getInteger(defaults.at("size")), 2048);
}
