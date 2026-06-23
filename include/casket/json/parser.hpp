#pragma once

#include <vector>
#include <charconv>
#include <system_error>
#include <casket/nonstd/optional.hpp>
#include <casket/json/value.hpp>
#include <casket/json/lexer.hpp>

namespace casket::json
{

class Parser
{
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    const Token& peek() const noexcept
    {
        static const Token eof(TokenType::Eof);
        return pos_ < tokens_.size() ? tokens_[pos_] : eof;
    }

    const Token& next()
    {
        if (pos_ < tokens_.size())
            return tokens_[pos_++];
        static const Token eof(TokenType::Eof);
        return eof;
    }

    const Token& expect(TokenType type)
    {
        const Token& tok = next();
        if (tok.type != type)
        {
            throw std::runtime_error("Expected token " + std::to_string(static_cast<int>(type)));
        }
        return tok;
    }

    nonstd::optional<Value> parseNumber(std::string_view str) const noexcept
    {
        Integer intVal;
        auto [ptr1, ec1] = std::from_chars(str.data(), str.data() + str.size(), intVal);
        if (ec1 == std::errc{} && ptr1 == str.data() + str.size())
        {
            return Value{intVal};
        }

        Float floatVal;
        auto [ptr2, ec2] = std::from_chars(str.data(), str.data() + str.size(), floatVal);
        if (ec2 == std::errc{} && ptr2 == str.data() + str.size())
        {
            return Value{floatVal};
        }

        return std::nullopt;
    }

public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens))
    {
    }

    Value parseValue();

    Value parseObject();

    Value parseArray();

    Value parse();
};

} // namespace casket::json