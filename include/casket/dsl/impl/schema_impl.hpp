#pragma once

#include <sstream>
#include <set>
#include <casket/dsl/schema.hpp>

namespace casket::dsl
{

inline const Value* Schema::getValueByPath(const Object* root, const std::string& path) const noexcept
{
    if (!root)
        return nullptr;

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.'))
        parts.push_back(part);

    const Object* current = root;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        auto it = current->fields.find(parts[i]);
        if (it == current->fields.end())
            return nullptr;

        const Value& val = it->second;
        if (i == parts.size() - 1)
            return &val;

        const auto* obj = val.get<Object>();
        if (!obj)
            return nullptr;
        current = obj;
    }

    return nullptr;
}

inline void Schema::setValueByPath(Object* root, const std::string& path, Value value)
{
    if (!root)
        return;

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.'))
        parts.push_back(part);

    Object* current = root;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i == parts.size() - 1)
        {
            current->fields[parts[i]] = std::move(value);
            return;
        }

        auto it = current->fields.find(parts[i]);
        if (it == current->fields.end() || !it->second.is<Object>())
        {
            current->fields[parts[i]] = Value{Object{}};
            it = current->fields.find(parts[i]);
        }

        auto* obj = it->second.get<Object>();
        if (!obj)
            return;
        current = obj;
    }
}

inline bool Schema::validate(Value& root, std::vector<std::string>& errors)
{
    if (!root.is<Object>())
    {
        errors.emplace_back("Root must be an object");
        return false;
    }

    auto* obj = root.get<Object>();
    if (!obj)
    {
        errors.emplace_back("Invalid object");
        return false;
    }

    appliedDefaults_.clear();
    std::vector<std::string> missing;

    for (const auto& spec : specs_)
    {
        const Value* value = getValueByPath(obj, std::string(spec->getPath()));

        if (spec->isRequired() && !value)
        {
            missing.push_back(std::string(spec->getPath()));
            continue;
        }

        if (value)
        {
            std::string error;
            if (!spec->validate(*value, error))
            {
                errors.push_back(std::move(error));
            }
        }

        if (!value && spec->getDefault().has_value())
        {
            Value def = std::move(*spec->getDefault());
            std::string path(spec->getPath());
            setValueByPath(obj, path, std::move(def));
            appliedDefaults_.emplace(path, *spec->getDefault());
        }
    }

    if (!missing.empty())
    {
        std::string msg = "Required parameters missing: ";
        for (size_t i = 0; i < missing.size(); ++i)
        {
            msg += missing[i];
            if (i < missing.size() - 1)
                msg += ", ";
        }
        errors.push_back(std::move(msg));
    }

    return errors.empty();
}

inline std::string Schema::generateHelp() const
{
    std::ostringstream ss;
    ss << "Parameters:\n" << std::string(60, '-') << "\n";
    for (const auto& spec : specs_)
    {
        ss << "  " << std::setw(25) << std::left << spec->getPath().data();
        ss << " - " << spec->getDescription().data();
        ss << " (type: " << spec->getTypeName() << ")";
        if (spec->isRequired())
            ss << " [required]";
        if (spec->getDefault().has_value())
        {
            ss << " (default: " << spec->getDefault()->toString() << ")";
        }
        ss << "\n";
    }
    return ss.str();
}

} // namespace casket::dsl