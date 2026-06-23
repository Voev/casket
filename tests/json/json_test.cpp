#include <gtest/gtest.h>
#include <casket/json/json.hpp>

using namespace casket::json;

class JsonTest : public ::testing::Test
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

TEST_F(JsonTest, IntegrationGenKeySimple)
{
    std::string input = "{ type: \"rsa\", size: 4096, output: \"key.pem\" }";
    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(getString(root->fields.at("type")), "rsa");
    EXPECT_EQ(getInteger(root->fields.at("size")), 4096);
    EXPECT_EQ(getString(root->fields.at("output")), "key.pem");
}

TEST_F(JsonTest, IntegrationGenKeyWithOptions)
{
    std::string input = R"(
        {
            type: "rsa",
            size: 2048,
            output: "secure.key",
            options: {
                encrypt: true,
                format: "pkcs8",
                iterations: 50000,
                cipher: "aes256",
                password: "secret123"
            }
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

TEST_F(JsonTest, IntegrationGenKeyWithMetadata)
{
    std::string input = R"(
        {
            type: "ec",
            size: 256,
            output: "ec-key.pem",
            metadata: {
                author: {
                    name: "John Doe",
                    email: "john@example.com"
                },
                environment: "prod",
                version: 2
            }
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

TEST_F(JsonTest, IntegrationFullGenKey)
{
    std::string input = R"(
        {
            type: "rsa",
            size: 4096,
            output: "keys/api-key.pem",
            name: "api-key",
            options: {
                encrypt: true,
                format: "pkcs8",
                iterations: 100000,
                cipher: "aes256",
                password: "super_secret"
            },
            metadata: {
                author: {
                    name: "John Doe",
                    email: "john@example.com"
                },
                environment: "prod",
                version: 3,
                purpose: "signing"
            },
            tags: ["api", "auth", "production"]
        }
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

TEST_F(JsonTest, IntegrationArrayOfObjects)
{
    std::string input = R"(
        [
            { name: "Alex", age: 30 },
            { name: "Bob", age: 25 }
        ]
    )";

    Value parsed = parseDSL(input);
    const auto* arr = getArray(parsed);
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->size(), 2);

    const auto* obj1 = getObject((*arr)[0]);
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(getString(obj1->fields.at("name")), "Alex");
    EXPECT_EQ(getInteger(obj1->fields.at("age")), 30);

    const auto* obj2 = getObject((*arr)[1]);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(getString(obj2->fields.at("name")), "Bob");
    EXPECT_EQ(getInteger(obj2->fields.at("age")), 25);
}

TEST_F(JsonTest, IntegrationNestedObjects)
{
    std::string input = R"(
        {
            app: {
                name: "myapp",
                db: {
                    host: "localhost",
                    port: 5432
                }
            }
        }
    )";

    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    auto* app = getObject(root->fields.at("app"));
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(getString(app->fields.at("name")), "myapp");

    auto* db = getObject(app->fields.at("db"));
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(getString(db->fields.at("host")), "localhost");
    EXPECT_EQ(getInteger(db->fields.at("port")), 5432);
}

TEST_F(JsonTest, IntegrationMixedTypes)
{
    std::string input = R"(
        {
            string: "hello",
            integer: 42,
            float: 3.14,
            boolean: true,
            null_value: null,
            array: [1, 2, 3],
            object: { key: "value" }
        }
    )";

    Value parsed = parseDSL(input);
    auto* root = getObject(parsed);
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(getString(root->fields.at("string")), "hello");
    EXPECT_EQ(getInteger(root->fields.at("integer")), 42);
    EXPECT_DOUBLE_EQ(getDouble(root->fields.at("float")), 3.14);
    EXPECT_TRUE(getBool(root->fields.at("boolean")));
    EXPECT_TRUE(root->fields.at("null_value").isNull());

    const auto* arr = getArray(root->fields.at("array"));
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->size(), 3);
    EXPECT_EQ(getInteger((*arr)[0]), 1);
    EXPECT_EQ(getInteger((*arr)[1]), 2);
    EXPECT_EQ(getInteger((*arr)[2]), 3);

    const auto* obj = getObject(root->fields.at("object"));
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(getString(obj->fields.at("key")), "value");
}

TEST_F(JsonTest, IntegrationArrayWithMixedTypes)
{
    std::string input = R"(
        [42, 3.14, true, "text", null, { key: "value" }, [1, 2, 3]]
    )";

    Value parsed = parseDSL(input);
    const auto* arr = getArray(parsed);
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->size(), 7);

    EXPECT_EQ(getInteger((*arr)[0]), 42);
    EXPECT_DOUBLE_EQ(getDouble((*arr)[1]), 3.14);
    EXPECT_TRUE(getBool((*arr)[2]));
    EXPECT_EQ(getString((*arr)[3]), "text");
    EXPECT_TRUE((*arr)[4].isNull());

    const auto* obj = getObject((*arr)[5]);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(getString(obj->fields.at("key")), "value");

    const auto* nestedArr = getArray((*arr)[6]);
    ASSERT_NE(nestedArr, nullptr);
    ASSERT_EQ(nestedArr->size(), 3);
    EXPECT_EQ(getInteger((*nestedArr)[0]), 1);
    EXPECT_EQ(getInteger((*nestedArr)[1]), 2);
    EXPECT_EQ(getInteger((*nestedArr)[2]), 3);
}

TEST_F(JsonTest, IntegrationEmptyObject)
{
    std::string input = "{}";
    Value parsed = parseDSL(input);
    const auto* obj = getObject(parsed);
    ASSERT_NE(obj, nullptr);
    EXPECT_TRUE(obj->empty());
}

TEST_F(JsonTest, IntegrationEmptyArray)
{
    std::string input = "[]";
    Value parsed = parseDSL(input);
    const auto* arr = getArray(parsed);
    ASSERT_NE(arr, nullptr);
    EXPECT_TRUE(arr->empty());
}

TEST_F(JsonTest, IntegrationSingleValue)
{
    Value parsed = parseDSL("hello");
    EXPECT_TRUE(parsed.is<std::string>());
    EXPECT_EQ(getString(parsed), "hello");

    parsed = parseDSL("42");
    EXPECT_TRUE(parsed.is<Integer>());
    EXPECT_EQ(getInteger(parsed), 42);

    parsed = parseDSL("3.14");
    EXPECT_TRUE(parsed.is<Float>());
    EXPECT_DOUBLE_EQ(getDouble(parsed), 3.14);

    parsed = parseDSL("true");
    EXPECT_TRUE(parsed.is<Boolean>());
    EXPECT_TRUE(getBool(parsed));

    parsed = parseDSL("null");
    EXPECT_TRUE(parsed.isNull());
}

TEST_F(JsonTest, ValueIsNull)
{
    Value v1;
    EXPECT_TRUE(v1.isNull());

    Value v2(Null{});
    EXPECT_TRUE(v2.isNull());

    Value v3("hello");
    EXPECT_FALSE(v3.isNull());
}

TEST_F(JsonTest, ValueTypeName)
{
    EXPECT_EQ(Value("test").typeName(), "string");
    EXPECT_EQ(Value(Integer(42)).typeName(), "integer");
    EXPECT_EQ(Value(Float(3.14)).typeName(), "float");
    EXPECT_EQ(Value(Boolean(true)).typeName(), "boolean");
    EXPECT_EQ(Value(Null{}).typeName(), "null");
    EXPECT_EQ(Value(Array{}).typeName(), "array");
    EXPECT_EQ(Value(Object{}).typeName(), "object");
}

TEST_F(JsonTest, ValueToString)
{
    EXPECT_EQ(Value("test").toString(), "\"test\"");
    EXPECT_EQ(Value(Integer(42)).toString(), "42");
    EXPECT_EQ(Value(Float(3.14)).toString(), "3.14");
    EXPECT_EQ(Value(Boolean(true)).toString(), "true");
    EXPECT_EQ(Value(Null{}).toString(), "null");
}

TEST_F(JsonTest, ValueAs)
{
    Value v = "hello";
    auto str = v.as<std::string>();
    EXPECT_TRUE(str.has_value());
    EXPECT_EQ(*str, "hello");

    auto intVal = v.as<Integer>();
    EXPECT_FALSE(intVal.has_value());
}

TEST_F(JsonTest, ObjectMethods)
{
    std::string input = R"(
        {
            name: "Alex",
            age: 30,
            active: true,
            tags: ["dev", "ops"]
        }
    )";

    Value parsed = parseDSL(input);
    auto* obj = getObject(parsed);
    ASSERT_NE(obj, nullptr);

    EXPECT_TRUE(obj->has("name"));
    EXPECT_TRUE(obj->has("age"));
    EXPECT_TRUE(obj->has("active"));
    EXPECT_TRUE(obj->has("tags"));
    EXPECT_FALSE(obj->has("missing"));

    EXPECT_EQ(obj->size(), 4);
    EXPECT_FALSE(obj->empty());

    auto name = obj->get<std::string>("name");
    EXPECT_TRUE(name.has_value());
    EXPECT_EQ(*name, "Alex");

    auto age = obj->get<Integer>("age");
    EXPECT_TRUE(age.has_value());
    EXPECT_EQ(*age, 30);

    EXPECT_EQ(obj->getOr<std::string>("name", "default"), "Alex");
    EXPECT_EQ(obj->getOr<std::string>("missing", "default"), "default");
    EXPECT_EQ(obj->getOr<Integer>("missing", 100), 100);
}

TEST_F(JsonTest, ArrayMethods)
{
    std::string input = "[1, 2, 3, 4, 5]";
    Value parsed = parseDSL(input);
    auto* arr = getArray(parsed);
    ASSERT_NE(arr, nullptr);

    EXPECT_EQ(arr->size(), 5);
    EXPECT_FALSE(arr->empty());

    EXPECT_EQ(getInteger((*arr)[0]), 1);
    EXPECT_EQ(getInteger((*arr)[1]), 2);
    EXPECT_EQ(getInteger((*arr)[2]), 3);
    EXPECT_EQ(getInteger((*arr)[3]), 4);
    EXPECT_EQ(getInteger((*arr)[4]), 5);

    // Test iteration
    int expected = 1;
    for (const auto& item : *arr)
    {
        EXPECT_EQ(getInteger(item), expected++);
    }
}