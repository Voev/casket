#include <sstream>
#include <gtest/gtest.h>
#include <casket/opt/opt.hpp>

using namespace casket::opt;

class OptionsReaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    ConfigOptions config;
    ConfigOptionsReader reader;
};

TEST_F(OptionsReaderTest, ReadSingleSection)
{
    class MainSection : public Section
    {
    public:
        MainSection()
        {
            addOption(OptionBuilder("key", Value<std::string>()).build());
        }

        static std::string name()
        {
            return "main";
        }
    };
    config.add<MainSection>();

    std::istringstream input("main {\nkey value\n}");
    ASSERT_NO_THROW(reader.read(input, config));

    auto option = config.get<MainSection>()->getOption("key");
    ASSERT_EQ(option.get<std::string>(), "value");
}

TEST_F(OptionsReaderTest, ReadMultipleSections)
{
    class MainSection : public Section
    {
    public:
        MainSection()
        {
            addOption(OptionBuilder("key1", Value<std::string>()).build());
            addOption(OptionBuilder("key2", Value<std::string>()).build());
        }

        static std::string name()
        {
            return "main";
        }
    };
    config.add<MainSection>();

    std::istringstream input("main {\nkey1 value1\n}\nmain {\nkey2 value2\n}");
    ASSERT_NO_THROW(reader.read(input, config));

    auto option = config.get<MainSection>()->getOption("key1");
    ASSERT_EQ(option.get<std::string>(), "value1");

    option = config.get<MainSection>()->getOption("key2");
    ASSERT_EQ(option.get<std::string>(), "value2");
}

TEST_F(OptionsReaderTest, MismatchedClosingBrace)
{
    std::istringstream input("main {\nkey value\n");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, UnknownSection)
{
    std::istringstream input("unknown_section {\nkey value\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, ReadSomeOptions)
{
    class MainSection : public Section
    {
    public:
        MainSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }

        static std::string name()
        {
            return "main";
        }
    };
    config.add<MainSection>();

    std::istringstream input("main {\nhost = localhost\nport = 9090\n}");
    ASSERT_NO_THROW(reader.read(input, config));

    auto host = config.get<MainSection>()->getOption("host");
    auto port = config.get<MainSection>()->getOption("port");
    ASSERT_EQ(host.get<std::string>(), "localhost");
    ASSERT_EQ(port.get<int>(), 9090);
}

TEST_F(OptionsReaderTest, ReadComments)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        # This is a comment
        main {  // inline comment after section
            host localhost  # line comment
            # port = 8080 - commented out
            port = 9090  // another comment
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));

    auto host = config.get<MainSection>()->getOption("host");
    auto port = config.get<MainSection>()->getOption("port");
    ASSERT_EQ(host.get<std::string>(), "localhost");
    ASSERT_EQ(port.get<int>(), 9090);
}

TEST_F(OptionsReaderTest, ReadNestedSections)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
        }
    };

    class NestedSection : public Section
    {
    public:
        static std::string name()
        {
            return "nested";
        }
        NestedSection()
        {
            addOption(OptionBuilder("key", Value<std::string>()).build());
        }
    };

    class SubNestedSection : public Section
    {
    public:
        static std::string name()
        {
            return "subnested";
        }
        SubNestedSection()
        {
            addOption(OptionBuilder("deep", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();
    config.add<NestedSection>();
    config.add<SubNestedSection>();

    std::istringstream input(R"(
        main {
            host localhost
            nested {
                key value
                subnested {
                    deep value
                }
            }
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));

    auto host = config.get<MainSection>()->getOption("host");
    ASSERT_EQ(host.get<std::string>(), "localhost");

    auto key = config.get<NestedSection>()->getOption("key");
    ASSERT_EQ(key.get<std::string>(), "value");

    auto deep = config.get<SubNestedSection>()->getOption("deep");
    ASSERT_EQ(deep.get<std::string>(), "value");
}

TEST_F(OptionsReaderTest, ReadMultilineStrings)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("description", Value<std::string>()).build());
            addOption(OptionBuilder("sql", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        main {
            description = """
                This is a multiline
                string that spans
                multiple lines
            """
            sql = '''
                SELECT * FROM users
                WHERE active = true
            '''
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadVariables)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("name", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        $app_name = "MyApp"
        $version = "1.0.0"
        
        main {
            name = ${app_name}_v${version}
            port = 9090
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));

    auto* section = config.get<MainSection>();
    ASSERT_NE(section, nullptr);

    auto name = section->getOption("name");
    EXPECT_EQ(name.get<std::string>(), "MyApp_v1.0.0");

    auto port = section->getOption("port");
    EXPECT_EQ(port.get<int>(), 9090);
}

TEST_F(OptionsReaderTest, ReadEnvironmentVariables)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("env_var", Value<std::string>()).build());
            addOption(OptionBuilder("simple_var", Value<std::string>()).build());
            addOption(OptionBuilder("host", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        main {
            env_var = ${TEST_ENV_VAR}
            simple_var = $TEST_ENV_VAR
            host = $TEST_HOST
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadQuotedValues)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("name", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();
    config.get<MainSection>()->addOption(OptionBuilder("message", Value<std::string>()).build());
    config.get<MainSection>()->addOption(OptionBuilder("path", Value<std::string>()).build());

    std::istringstream input(R"(
        main {
            message = "Hello World"
            path = 'C:\\Program Files\\App'
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadEscapedCharacters)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("name", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();
    config.get<MainSection>()->addOption(OptionBuilder("windows_path", Value<std::string>()).build());
    config.get<MainSection>()->addOption(OptionBuilder("regex", Value<std::string>()).build());

    std::istringstream input(R"(
        main {
            windows_path = "C:\\Program Files\\App"
            regex = "\\(\\d{3}\\) \\d{3}-\\d{4}"
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadArrays)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("name", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();
    config.get<MainSection>()->addOption(OptionBuilder("ports", MultiValue<int>()).build());
    config.get<MainSection>()->addOption(OptionBuilder("hosts", MultiValue<std::string>()).build());

    std::istringstream input(R"(
        main {
            ports = [8080, 8081, 8082]
            hosts = ["localhost", "127.0.0.1", "::1"]
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadBooleanValues)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("flag1", Value<bool>()).build());
            addOption(OptionBuilder("flag2", Value<bool>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        main {
            flag1 = true
            flag2 = false
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadInclude)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
        }
    };

    class OtherSection : public Section
    {
    public:
        static std::string name()
        {
            return "other";
        }
        OtherSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();
    config.add<OtherSection>();

    std::ofstream includeFile("test_include.conf");
    includeFile << "other {\n    value = included_value\n}\n";
    includeFile.close();

    std::istringstream input(R"(
        include "test_include.conf"
        main {
            host localhost
        }
    )");

    reader.addIncludePath(".");
    ASSERT_NO_THROW(reader.read(input, config));

    std::remove("test_include.conf");
}

TEST_F(OptionsReaderTest, ReadIncludeWithEnvVar)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("name", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    class OtherSection : public Section
    {
    public:
        static std::string name()
        {
            return "other";
        }
        OtherSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();
    config.add<OtherSection>();

    // Create include file in current directory
    std::ofstream includeFile("included.conf");
    includeFile << "other {\n    value = included\n}\n";
    includeFile.close();

    // Get current directory for include path
    std::string currentDir = ".";

    std::istringstream input(R"(
        include "included.conf"
    )");

    reader.addIncludePath(currentDir);
    reader.setEnableVariables(true); // Ensure variables are enabled

    ASSERT_NO_THROW(reader.read(input, config));

    // Verify the included section was parsed
    auto* otherSection = config.get<OtherSection>();
    ASSERT_NE(otherSection, nullptr);

    auto value = otherSection->getOption("value");
    EXPECT_EQ(value.get<std::string>(), "included");

    // Clean up
    std::remove("included.conf");
}

TEST_F(OptionsReaderTest, ReadVariableScope)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    class OtherSection : public Section
    {
    public:
        static std::string name()
        {
            return "other";
        }
        OtherSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();
    config.add<OtherSection>();

    std::istringstream input(R"(
        $global = "global_value"
        
        main {
            $local = "local_value"
            
            value = $global
        }
        
        other {
            value = $global
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, MissingClosingBrace)
{
    std::istringstream input("main {\nkey value\n");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, ExtraClosingBrace)
{
    std::istringstream input("main {\nkey value\n}\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, InvalidVariableDefinition)
{
    std::istringstream input("$invalid_var\nmain {\nkey value\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, EmptySection)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }

        MainSection()
        {
        }
    };
    config.add<MainSection>();
    std::istringstream input("main {\n}");
    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ExtraWhitespace)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        
        main    {
            
            host      localhost
            
            
            port     =    9090
            
        }
        
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, DisableComments)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();

    reader.setEnableComments(false);

    std::istringstream input("main {\n#this_will_be_value\nvalue test\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, DisableVariables)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
        }
    };

    config.add<MainSection>();
    reader.setEnableVariables(false);

    std::istringstream input("main {\n$var = test\nvalue = $var\n}");
    ASSERT_THROW(reader.read(input, config), std::runtime_error);
}

TEST_F(OptionsReaderTest, DisableEqualsConversion)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("key=value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();
    reader.setConvertEqualsToSpace(false);

    std::istringstream input("main {\nkey=value test\n}");
    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, QuotedSectionNames)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("value", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        "main" {
            value = test
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, SpecialCharacters)
{
    class MainSection : public Section
    {
    public:
        static std::string name()
        {
            return "main";
        }
        MainSection()
        {
            addOption(OptionBuilder("email", Value<std::string>()).build());
            addOption(OptionBuilder("url", Value<std::string>()).build());
        }
    };

    config.add<MainSection>();

    std::istringstream input(R"(
        main {
            email = "user@example.com"
            url = "https://example.com/path?query=value&param=test"
        }
    )");

    ASSERT_NO_THROW(reader.read(input, config));
}

TEST_F(OptionsReaderTest, ReadEnvironmentVariableExpansion)
{
    class EnvTestSection : public Section
    {
    public:
        static std::string name()
        {
            return "env_test";
        }
        EnvTestSection()
        {
            addOption(OptionBuilder("host", Value<std::string>()).build());
            addOption(OptionBuilder("port", Value<int>()).build());
            addOption(OptionBuilder("path", Value<std::string>()).build());
            addOption(OptionBuilder("message", Value<std::string>()).build());
        }
    };

    config.add<EnvTestSection>();

    // Set test environment variables
#ifdef _WIN32
    _putenv("TEST_HOST=production.example.com");
    _putenv("TEST_PORT=9090");
    _putenv("TEST_PATH=/usr/local/app");
#else
    setenv("TEST_HOST", "production.example.com", 1);
    setenv("TEST_PORT", "9090", 1);
    setenv("TEST_PATH", "/usr/local/app", 1);
#endif

    std::istringstream input(R"(
        env_test {
            host = ${TEST_HOST}
            port = ${TEST_PORT}
            path = $TEST_PATH
            message = "Server running on ${TEST_HOST}:${TEST_PORT}"
        }
    )");

    reader.setEnableVariables(true);
    ASSERT_NO_THROW(reader.read(input, config));

    // Verify environment variables were expanded
    auto* section = config.get<EnvTestSection>();
    ASSERT_NE(section, nullptr);

    // Test with braces syntax
    auto host = section->getOption("host");
    EXPECT_EQ(host.get<std::string>(), "production.example.com");

    // Test with braces syntax for integer
    auto port = section->getOption("port");
    EXPECT_EQ(port.get<int>(), 9090);

    // Test without braces syntax
    auto path = section->getOption("path");
    EXPECT_EQ(path.get<std::string>(), "/usr/local/app");

    // Test in quoted string
    auto message = section->getOption("message");
    EXPECT_EQ(message.get<std::string>(), "Server running on production.example.com:9090");

    // Clean up environment variables
#ifdef _WIN32
    _putenv("TEST_HOST=");
    _putenv("TEST_PORT=");
    _putenv("TEST_PATH=");
#else
    unsetenv("TEST_HOST");
    unsetenv("TEST_PORT");
    unsetenv("TEST_PATH");
#endif
}