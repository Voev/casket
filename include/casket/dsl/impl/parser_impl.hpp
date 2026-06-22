#pragma once

#include <casket/dsl/parser.hpp>

namespace casket::dsl
{

inline Value Parser::parseValue()
{
    const Token& tok = peek();

    if (tok.type == TokenType::Text)
    {
        next();
        std::string_view val = tok.value;

        if (val == "true")
            return Value{true};
        if (val == "false")
            return Value{false};
        if (val == "null")
            return Value{Null{}};
        if (auto num = parseNumber(val))
            return std::move(*num);
        return Value{std::string(val)};
    }
    else if (tok.type == TokenType::LBrace)
    {
        next();
        return parseObject();
    }
    else if (tok.type == TokenType::LBracket)
    {
        next();
        return parseArray();
    }

    throw std::runtime_error("Unexpected token: " + tok.value);
}

inline Value Parser::parseObject()
{
    Object obj;

    while (peek().type != TokenType::RBrace && peek().type != TokenType::Eof)
    {
        std::string key = std::move(expect(TokenType::Text).value);
        expect(TokenType::Eq);
        obj.fields.emplace(std::move(key), parseValue());
    }

    if (peek().type == TokenType::RBrace)
        next();

    return Value{std::move(obj)};
}

inline Value Parser::parseArray()
{
    Array arr;
    arr.reserve(16);

    while (peek().type != TokenType::RBracket && peek().type != TokenType::Eof)
    {
        arr.push_back(parseValue());
    }

    if (peek().type == TokenType::RBracket)
        next();

    return Value{std::move(arr)};
}

inline Value Parser::parse()
{
    Value result = parseValue();
    if (peek().type != TokenType::Eof)
    {
        throw std::runtime_error("Extra tokens after parsing");
    }
    return result;
}

// ============================================================================
// parseDSL
// ============================================================================
inline Value parseDSL(std::string_view input)
{
    Lexer lexer(input);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

} // namespace casket::dsl