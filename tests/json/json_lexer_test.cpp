#include <gtest/gtest.h>
#include <casket/json/json.hpp>

using namespace casket::json;

class JsonLexerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(JsonLexerTest, LexerSimpleTokens)
{
    Lexer lexer("key: value");
    auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "key");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "value");
    EXPECT_EQ(tokens[3].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerObjectTokens)
{
    Lexer lexer("{ name: \"Alex\" }");
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 6);
    EXPECT_EQ(tokens[0].type, TokenType::LBrace);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "name");
    EXPECT_EQ(tokens[2].type, TokenType::Colon);
    EXPECT_EQ(tokens[3].type, TokenType::Text);
    EXPECT_EQ(tokens[3].value, "Alex");
    EXPECT_EQ(tokens[4].type, TokenType::RBrace);
    EXPECT_EQ(tokens[5].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerObjectWithComma)
{
    Lexer lexer("{ name: \"Alex\", age: 30 }");
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 10);
    EXPECT_EQ(tokens[0].type, TokenType::LBrace);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "name");
    EXPECT_EQ(tokens[2].type, TokenType::Colon);
    EXPECT_EQ(tokens[3].type, TokenType::Text);
    EXPECT_EQ(tokens[3].value, "Alex");
    EXPECT_EQ(tokens[4].type, TokenType::Comma);
    EXPECT_EQ(tokens[5].type, TokenType::Text);
    EXPECT_EQ(tokens[5].value, "age");
    EXPECT_EQ(tokens[6].type, TokenType::Colon);
    EXPECT_EQ(tokens[7].type, TokenType::Text);
    EXPECT_EQ(tokens[7].value, "30");
    EXPECT_EQ(tokens[8].type, TokenType::RBrace);
    EXPECT_EQ(tokens[9].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerArrayTokens)
{
    Lexer lexer("[\"dev\", \"ops\"]");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::LBracket);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "dev");
    EXPECT_EQ(tokens[2].type, TokenType::Comma);
    EXPECT_EQ(tokens[3].type, TokenType::Text);
    EXPECT_EQ(tokens[3].value, "ops");
    EXPECT_EQ(tokens[4].type, TokenType::RBracket);
    EXPECT_EQ(tokens[5].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerArrayWithoutComma)
{
    Lexer lexer("[\"dev\" \"ops\"]");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::LBracket);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "dev");
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "ops");
    EXPECT_EQ(tokens[3].type, TokenType::RBracket);
    EXPECT_EQ(tokens[4].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerQuotedString)
{
    Lexer lexer("name: \"John Doe\"");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "name");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "John Doe");
}

TEST_F(JsonLexerTest, LexerSingleQuotedString)
{
    Lexer lexer("name: 'John Doe'");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "name");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "John Doe");
}

TEST_F(JsonLexerTest, LexerEscapeSequences)
{
    Lexer lexer("text: \"Hello\\nWorld\\tTab\"");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[2].value, "Hello\nWorld\tTab");
}

TEST_F(JsonLexerTest, LexerLineAndColumn)
{
    Lexer lexer("key: value\nnext: 123");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    EXPECT_EQ(tokens[2].line, 1);
    EXPECT_EQ(tokens[2].column, 6);

    // Second line
    EXPECT_EQ(tokens[5].line, 2);
    EXPECT_EQ(tokens[5].column, 7);
}

TEST_F(JsonLexerTest, LexerNestedObject)
{
    Lexer lexer("{ app: { name: \"myapp\", port: 8080 } }");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::LBrace);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "app");
    EXPECT_EQ(tokens[2].type, TokenType::Colon);
    EXPECT_EQ(tokens[3].type, TokenType::LBrace);
    EXPECT_EQ(tokens[4].type, TokenType::Text);
    EXPECT_EQ(tokens[4].value, "name");
    EXPECT_EQ(tokens[5].type, TokenType::Colon);
    EXPECT_EQ(tokens[6].type, TokenType::Text);
    EXPECT_EQ(tokens[6].value, "myapp");
    EXPECT_EQ(tokens[7].type, TokenType::Comma);
    EXPECT_EQ(tokens[8].type, TokenType::Text);
    EXPECT_EQ(tokens[8].value, "port");
    EXPECT_EQ(tokens[9].type, TokenType::Colon);
    EXPECT_EQ(tokens[10].type, TokenType::Text);
    EXPECT_EQ(tokens[10].value, "8080");
    EXPECT_EQ(tokens[11].type, TokenType::RBrace);
    EXPECT_EQ(tokens[12].type, TokenType::RBrace);
    EXPECT_EQ(tokens[13].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerMixedTypes)
{
    Lexer lexer("{ name: \"myapp\", port: 8080, debug: true, tags: [\"dev\", \"ops\"] }");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::LBrace);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].value, "name");
    EXPECT_EQ(tokens[2].type, TokenType::Colon);
    EXPECT_EQ(tokens[3].type, TokenType::Text);
    EXPECT_EQ(tokens[3].value, "myapp");
    EXPECT_EQ(tokens[4].type, TokenType::Comma);
    EXPECT_EQ(tokens[5].type, TokenType::Text);
    EXPECT_EQ(tokens[5].value, "port");
    EXPECT_EQ(tokens[6].type, TokenType::Colon);
    EXPECT_EQ(tokens[7].type, TokenType::Text);
    EXPECT_EQ(tokens[7].value, "8080");
    EXPECT_EQ(tokens[8].type, TokenType::Comma);
    EXPECT_EQ(tokens[9].type, TokenType::Text);
    EXPECT_EQ(tokens[9].value, "debug");
    EXPECT_EQ(tokens[10].type, TokenType::Colon);
    EXPECT_EQ(tokens[11].type, TokenType::Text);
    EXPECT_EQ(tokens[11].value, "true");
    EXPECT_EQ(tokens[12].type, TokenType::Comma);
    EXPECT_EQ(tokens[13].type, TokenType::Text);
    EXPECT_EQ(tokens[13].value, "tags");
    EXPECT_EQ(tokens[14].type, TokenType::Colon);
    EXPECT_EQ(tokens[15].type, TokenType::LBracket);
    EXPECT_EQ(tokens[16].type, TokenType::Text);
    EXPECT_EQ(tokens[16].value, "dev");
    EXPECT_EQ(tokens[17].type, TokenType::Comma);
    EXPECT_EQ(tokens[18].type, TokenType::Text);
    EXPECT_EQ(tokens[18].value, "ops");
    EXPECT_EQ(tokens[19].type, TokenType::RBracket);
    EXPECT_EQ(tokens[20].type, TokenType::RBrace);
    EXPECT_EQ(tokens[21].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerNumberTokens)
{
    Lexer lexer("count: 42, pi: 3.14, science: 1e-5");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "count");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "42");
    EXPECT_EQ(tokens[3].type, TokenType::Comma);
    EXPECT_EQ(tokens[4].type, TokenType::Text);
    EXPECT_EQ(tokens[4].value, "pi");
    EXPECT_EQ(tokens[5].type, TokenType::Colon);
    EXPECT_EQ(tokens[6].type, TokenType::Text);
    EXPECT_EQ(tokens[6].value, "3.14");
    EXPECT_EQ(tokens[7].type, TokenType::Comma);
    EXPECT_EQ(tokens[8].type, TokenType::Text);
    EXPECT_EQ(tokens[8].value, "science");
    EXPECT_EQ(tokens[9].type, TokenType::Colon);
    EXPECT_EQ(tokens[10].type, TokenType::Text);
    EXPECT_EQ(tokens[10].value, "1e-5");
    EXPECT_EQ(tokens[11].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerBooleanTokens)
{
    Lexer lexer("enabled: true, disabled: false");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "enabled");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "true");
    EXPECT_EQ(tokens[3].type, TokenType::Comma);
    EXPECT_EQ(tokens[4].type, TokenType::Text);
    EXPECT_EQ(tokens[4].value, "disabled");
    EXPECT_EQ(tokens[5].type, TokenType::Colon);
    EXPECT_EQ(tokens[6].type, TokenType::Text);
    EXPECT_EQ(tokens[6].value, "false");
    EXPECT_EQ(tokens[7].type, TokenType::Eof);
}

TEST_F(JsonLexerTest, LexerNullToken)
{
    Lexer lexer("value: null");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "value");
    EXPECT_EQ(tokens[1].type, TokenType::Colon);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "null");
    EXPECT_EQ(tokens[3].type, TokenType::Eof);
}