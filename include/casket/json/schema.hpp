#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include <casket/json/value.hpp>

namespace casket::json
{

// ============================================================================
// TypedParamSpec
// ============================================================================
template <typename T>
struct TypedParamSpec
{
    std::string path;
    std::string description;
    bool requiredValue = false;
    std::optional<T> defaultValue;
    std::function<bool(const T&)> validator;

    TypedParamSpec() = default;

    TypedParamSpec(std::string p, std::string desc, bool req = false, std::optional<T> def = std::nullopt)
        : path(std::move(p))
        , description(std::move(desc))
        , requiredValue(req)
        , defaultValue(std::move(def))
    {
    }

    TypedParamSpec& withValidator(std::function<bool(const T&)> v)
    {
        validator = std::move(v);
        return *this;
    }

    template <typename U = T>
    TypedParamSpec& withAllowedValues(const std::vector<std::string>& allowed)
    {
        static_assert(std::is_same_v<U, std::string>, "withAllowedValues is only for std::string");
        validator = [allowed](const std::string& val)
        {
            return std::any_of(allowed.begin(),
                               allowed.end(),
                               [&val](const auto& a)
                               {
                                   return a == val;
                               });
        };
        return *this;
    }

    template <typename U = T>
    TypedParamSpec& withRange(U min, U max)
    {
        static_assert(std::is_arithmetic_v<U>, "withRange is only for arithmetic types");
        validator = [min, max](const U& val)
        {
            return val >= min && val <= max;
        };
        return *this;
    }

    template <typename U = T>
    TypedParamSpec& withMin(U min)
    {
        static_assert(std::is_arithmetic_v<U>, "withMin is only for arithmetic types");
        validator = [min](const U& val)
        {
            return val >= min;
        };
        return *this;
    }

    template <typename U = T>
    TypedParamSpec& withMax(U max)
    {
        static_assert(std::is_arithmetic_v<U>, "withMax is only for arithmetic types");
        validator = [max](const U& val)
        {
            return val <= max;
        };
        return *this;
    }

    TypedParamSpec& required(bool req = true)
    {
        requiredValue = req;
        return *this;
    }
    TypedParamSpec& withDefault(T def)
    {
        defaultValue = std::move(def);
        return *this;
    }
    TypedParamSpec& withPath(std::string p)
    {
        path = std::move(p);
        return *this;
    }
    TypedParamSpec& withDescription(std::string desc)
    {
        description = std::move(desc);
        return *this;
    }
};

// ============================================================================
// ISpec
// ============================================================================
class ISpec
{
public:
    virtual ~ISpec() = default;
    virtual std::string_view getPath() const = 0;
    virtual std::string_view getDescription() const = 0;
    virtual bool isRequired() const = 0;
    virtual bool validate(const Value& v, std::string& error) const = 0;
    virtual std::optional<Value> getDefault() const = 0;
    virtual std::string getTypeName() const = 0;
};

template <typename T>
class SpecImpl final : public ISpec
{
    TypedParamSpec<T> spec_;

public:
    explicit SpecImpl(TypedParamSpec<T> spec)
        : spec_(std::move(spec))
    {
    }

    std::string_view getPath() const override
    {
        return spec_.path;
    }
    std::string_view getDescription() const override
    {
        return spec_.description;
    }
    bool isRequired() const override
    {
        return spec_.requiredValue;
    }

    bool validate(const Value& v, std::string& error) const override
    {
        const T* val = v.get<T>();
        if (!val)
        {
            error = "Expected " + getTypeName() + ", got " + v.typeName();
            return false;
        }
        if (spec_.validator && !spec_.validator(*val))
        {
            error = "Validation failed for '" + spec_.path + "'";
            return false;
        }
        return true;
    }

    std::optional<Value> getDefault() const override
    {
        if (spec_.defaultValue.has_value())
            return Value{*spec_.defaultValue};
        return std::nullopt;
    }

    std::string getTypeName() const override
    {
        if constexpr (std::is_same_v<T, std::string>)
            return "string";
        else if constexpr (std::is_same_v<T, Integer>)
            return "integer";
        else if constexpr (std::is_same_v<T, Float>)
            return "float";
        else if constexpr (std::is_same_v<T, Boolean>)
            return "boolean";
        else if constexpr (std::is_same_v<T, Null>)
            return "null";
        else if constexpr (std::is_same_v<T, Array>)
            return "array";
        else if constexpr (std::is_same_v<T, Object>)
            return "object";
        else
            return "unknown";
    }
};

// ============================================================================
// Schema
// ============================================================================
class Schema
{
    std::vector<std::unique_ptr<ISpec>> specs_;
    std::unordered_map<std::string, Value> appliedDefaults_;

    const Value* getValueByPath(const Object* root, const std::string& path) const noexcept;

    void setValueByPath(Object* root, const std::string& path, Value value);

public:
    Schema() = default;

    template <typename T>
    Schema& add(TypedParamSpec<T> spec)
    {
        specs_.push_back(std::make_unique<SpecImpl<T>>(std::move(spec)));
        return *this;
    }

    bool validate(Value& root, std::vector<std::string>& errors);

    const std::unordered_map<std::string, Value>& getAppliedDefaults() const noexcept
    {
        return appliedDefaults_;
    }

    std::string generateHelp() const;
};

} // namespace casket::json