#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cctype>
#include <stdexcept>

namespace casket::dsl
{

// ============================================================================
// Token
// ============================================================================
enum class TokenType
{
    Text,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Eq,
    Eof
};

struct Token
{
    TokenType type;
    std::string value;
    size_t line;
    size_t column;

    Token(TokenType t, std::string v = "", size_t l = 1, size_t c = 1) noexcept
        : type(t)
        , value(std::move(v))
        , line(l)
        , column(c)
    {
    }

    Token(const Token&) = default;
    Token(Token&&) noexcept = default;
    Token& operator=(const Token&) = default;
    Token& operator=(Token&&) noexcept = default;
};

// ============================================================================
// Lexer
// ============================================================================
class Lexer
{
    std::string_view input_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;

    char peek() const noexcept
    {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char get() noexcept
    {
        char ch = pos_ < input_.size() ? input_[pos_++] : '\0';
        if (ch == '\n')
        {
            line_++;
            column_ = 1;
        }
        else
        {
            column_++;
        }
        return ch;
    }

    void skipWhitespace() noexcept
    {
        while (std::isspace(static_cast<unsigned char>(peek())))
            get();
    }

    std::string readQuotedString(char quote)
    {
        get();
        std::string result;
        result.reserve(64);

        while (peek() != '\0' && peek() != quote)
        {
            if (peek() == '\\')
            {
                get();
                char escaped = get();
                switch (escaped)
                {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '"':
                    result += '"';
                    break;
                case '\'':
                    result += '\'';
                    break;
                default:
                    result += escaped;
                    break;
                }
            }
            else
            {
                result += get();
            }
        }
        if (peek() == quote)
            get();
        return result;
    }

    std::string readText()
    {
        size_t start = pos_;
        while (pos_ < input_.size() && !std::isspace(static_cast<unsigned char>(input_[pos_])) && input_[pos_] != '{' &&
               input_[pos_] != '}' && input_[pos_] != '[' && input_[pos_] != ']' && input_[pos_] != '=')
        {
            pos_++;
            column_++;
        }
        return std::string(input_.substr(start, pos_ - start));
    }

public:
    explicit Lexer(std::string_view input) noexcept
        : input_(input)
    {
    }

    std::vector<Token> tokenize()
    {
        std::vector<Token> tokens;
        tokens.reserve(128);

        while (true)
        {
            skipWhitespace();
            char ch = peek();
            if (ch == '\0')
            {
                tokens.emplace_back(TokenType::Eof, "", line_, column_);
                break;
            }

            size_t tokLine = line_, tokCol = column_;
            switch (ch)
            {
            case '{':
                tokens.emplace_back(TokenType::LBrace, "{", tokLine, tokCol);
                get();
                break;
            case '}':
                tokens.emplace_back(TokenType::RBrace, "}", tokLine, tokCol);
                get();
                break;
            case '[':
                tokens.emplace_back(TokenType::LBracket, "[", tokLine, tokCol);
                get();
                break;
            case ']':
                tokens.emplace_back(TokenType::RBracket, "]", tokLine, tokCol);
                get();
                break;
            case '=':
                tokens.emplace_back(TokenType::Eq, "=", tokLine, tokCol);
                get();
                break;
            case '"':
            case '\'':
                tokens.emplace_back(TokenType::Text, readQuotedString(ch), tokLine, tokCol);
                break;
            default:
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == '.' || ch == '/')
                {
                    tokens.emplace_back(TokenType::Text, readText(), tokLine, tokCol);
                }
                else
                {
                    throw std::runtime_error("Unknown character '" + std::string(1, ch) + "' at line " +
                                             std::to_string(line_));
                }
            }
        }
        return tokens;
    }
};

} // namespace casket::dsl