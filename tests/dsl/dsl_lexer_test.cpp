#include <gtest/gtest.h>
#include <casket/dsl/dsl.hpp>

using namespace casket::dsl;

class DslLexerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(DslLexerTest, LexerSimpleTokens)
{
    Lexer lexer("key=value");
    auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "key");
    EXPECT_EQ(tokens[1].type, TokenType::Eq);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "value");
    EXPECT_EQ(tokens[3].type, TokenType::Eof);
}

TEST_F(DslLexerTest, LexerObjectTokens)
{
    Lexer lexer("user { name=Alex }");
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 7);
    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "user");
    EXPECT_EQ(tokens[1].type, TokenType::LBrace);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "name");
    EXPECT_EQ(tokens[3].type, TokenType::Eq);
    EXPECT_EQ(tokens[4].type, TokenType::Text);
    EXPECT_EQ(tokens[4].value, "Alex");
    EXPECT_EQ(tokens[5].type, TokenType::RBrace);
    EXPECT_EQ(tokens[6].type, TokenType::Eof);
}

TEST_F(DslLexerTest, LexerArrayTokens)
{
    Lexer lexer("tags=[dev ops]");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "tags");
    EXPECT_EQ(tokens[1].type, TokenType::Eq);
    EXPECT_EQ(tokens[2].type, TokenType::LBracket);
    EXPECT_EQ(tokens[3].type, TokenType::Text);
    EXPECT_EQ(tokens[3].value, "dev");
    EXPECT_EQ(tokens[4].type, TokenType::Text);
    EXPECT_EQ(tokens[4].value, "ops");
    EXPECT_EQ(tokens[5].type, TokenType::RBracket);
    EXPECT_EQ(tokens[6].type, TokenType::Eof);
}

TEST_F(DslLexerTest, LexerQuotedString)
{
    Lexer lexer("name=\"John Doe\"");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Text);
    EXPECT_EQ(tokens[0].value, "name");
    EXPECT_EQ(tokens[1].type, TokenType::Eq);
    EXPECT_EQ(tokens[2].type, TokenType::Text);
    EXPECT_EQ(tokens[2].value, "John Doe");
}

TEST_F(DslLexerTest, LexerEscapeSequences)
{
    Lexer lexer("text=\"Hello\\nWorld\\tTab\"");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[2].value, "Hello\nWorld\tTab");
}

TEST_F(DslLexerTest, LexerLineAndColumn)
{
    Lexer lexer("key=value\nnext=123");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    EXPECT_EQ(tokens[2].line, 1);
    EXPECT_EQ(tokens[2].column, 5);

    // Second line
    EXPECT_EQ(tokens[5].line, 2);
    EXPECT_EQ(tokens[5].column, 1);
}
