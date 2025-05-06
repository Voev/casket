#include <gtest/gtest.h>
#include <casket/opt/config_options.hpp>

using namespace casket::opt;

class TestSection : public Section {
public:
    static std::string name() {
        return "TestSection";
    }
};

class AnotherSection : public Section {
public:
    static std::string name() {
        return "AnotherSection";
    }
};

class ConfigTest : public ::testing::Test {
protected:
    ConfigOptions config;
};

TEST_F(ConfigTest, AddSection) {
    EXPECT_NO_THROW(config.add<TestSection>());
    EXPECT_NE(config.find("TestSection"), nullptr);
}

TEST_F(ConfigTest, AddDuplicateSection) {
    config.add<TestSection>();
    EXPECT_THROW(config.add<TestSection>(), std::runtime_error);
}

TEST_F(ConfigTest, GetSection) {
    config.add<TestSection>();
    auto section = config.get<TestSection>();
    EXPECT_NE(section, nullptr);
}

TEST_F(ConfigTest, GetNonExistentSection) {
    EXPECT_THROW(config.get<TestSection>(), std::runtime_error);
}

TEST_F(ConfigTest, FindSection) {
    config.add<TestSection>();
    auto section = config.find("TestSection");
    EXPECT_NE(section, nullptr);
}

TEST_F(ConfigTest, FindNonExistentSection) {
    auto section = config.find("NonExistentSection");
    EXPECT_EQ(section, nullptr);
}
