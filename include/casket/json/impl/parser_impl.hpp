#pragma once

#include <casket/json/parser.hpp>

namespace casket::json
{

inline Value Parser::parseValue()
{
    const Token& tok = peek();

    if (tok.type == TokenType::Text)
    {
        std::string val = std::move(peek().value);
        next();

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
    bool first = true;

    while (peek().type != TokenType::RBrace && peek().type != TokenType::Eof)
    {
        if (!first && peek().type == TokenType::Comma)
        {
            next();
        }
        first = false;

        // Parse key
        std::string key = std::move(expect(TokenType::Text).value);
        
        // Expect ':'
        expect(TokenType::Colon);
        
        // Parse value
        obj.fields.emplace(std::move(key), parseValue());
    }

    if (peek().type == TokenType::Eof)
    {
        throw std::runtime_error("Unclosed object: expected '}'");
    }

    if (peek().type == TokenType::RBrace)
    {
        next();
    }

    return Value{std::move(obj)};
}

inline Value Parser::parseArray()
{
    Array arr;
    bool first = true;

    while (peek().type != TokenType::RBracket && peek().type != TokenType::Eof)
    {
        if (!first && peek().type == TokenType::Comma)
        {
            next();
        }

        first = false;

        arr.push_back(parseValue());
    }

    if (peek().type == TokenType::Eof)
    {
        throw std::runtime_error("Unclosed array: expected ']'");
    }

    if (peek().type == TokenType::RBracket)
    {
        next();
    }

    return Value{std::move(arr)};
}

inline Value Parser::parse()
{
    Value result;

    // Handle root-level object: "{ name: Alex }"
    if (peek().type == TokenType::LBrace)
    {
        next();
        result = parseObject();
    }
    // Handle root-level array: "[1, 2, 3]"
    else if (peek().type == TokenType::LBracket)
    {
        next();
        result = parseArray();
    }
    // Handle root-level plain value
    else if (peek().type == TokenType::Text)
    {
        std::string val = std::move(peek().value);
        next();

        if (val == "true")
            result = Value{true};
        else if (val == "false")
            result = Value{false};
        else if (val == "null")
            result = Value{Null{}};
        else if (auto num = parseNumber(val))
            result = std::move(*num);
        else
            result = Value{std::move(val)};
    }
    else
    {
        throw std::runtime_error("Unexpected token at root level: " + peek().value);
    }

    if (peek().type != TokenType::Eof)
    {
        throw std::runtime_error("Extra tokens after parsing: '" + peek().value + 
                                 "' at line " + std::to_string(peek().line) + 
                                 ", column " + std::to_string(peek().column));
    }

    return result;
}

inline Value parseDSL(std::string_view input)
{
    Lexer lexer(input);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

} // namespace casket::json