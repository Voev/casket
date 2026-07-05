#pragma once
#include <casket/db/i_row.hpp>
#include <casket/db/typed_field_value.hpp>
#include <casket/db/exception.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace casket::db
{

template <typename T>
class FieldExtractor
{
public:
    static T extract(const std::shared_ptr<IFieldValue>& field)
    {
        if (!field || field->isNull())
        {
            throw Exception(ErrorType::INVALID_CONVERSION, "Field is null");
        }

        auto typed = std::dynamic_pointer_cast<TypedFieldValue<T>>(field);
        if (typed)
        {
            return typed->getValue();
        }

        throw Exception(ErrorType::TYPE_MISMATCH, "Cannot convert field to requested type");
    }
};

template <>
class FieldExtractor<std::vector<uint8_t>>
{
public:
    static std::vector<uint8_t> extract(const std::shared_ptr<IFieldValue>& field)
    {
        if (!field || field->isNull())
        {
            return std::vector<uint8_t>();
        }

        auto binaryField = std::dynamic_pointer_cast<BinaryDataField>(field);
        if (binaryField)
        {
            return binaryField->getValue();
        }

        auto strField = std::dynamic_pointer_cast<TypedFieldValue<std::string>>(field);
        if (strField)
        {
            std::string hex = strField->getValue();
            if (hex.empty() || hex == "NULL")
            {
                return std::vector<uint8_t>();
            }
            std::vector<uint8_t> data;
            data.reserve(hex.length() / 2);
            for (size_t i = 0; i + 1 < hex.length(); i += 2)
            {
                uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
                data.push_back(byte);
            }
            return data;
        }

        throw Exception(ErrorType::TYPE_MISMATCH, "Cannot convert field to vector<uint8_t>");
    }
};

template <>
class FieldExtractor<std::chrono::system_clock::time_point>
{
public:
    static std::chrono::system_clock::time_point extract(const std::shared_ptr<IFieldValue>& field)
    {
        if (!field || field->isNull())
        {
            throw Exception(ErrorType::INVALID_CONVERSION, "Field is null");
        }

        auto timeField = std::dynamic_pointer_cast<TimePointField>(field);
        if (timeField)
        {
            return timeField->getValue();
        }

        auto strField = std::dynamic_pointer_cast<TypedFieldValue<std::string>>(field);
        if (strField)
        {
            std::string str = strField->getValue();
            if (str == "NULL" || str.empty())
            {
                throw Exception(ErrorType::INVALID_CONVERSION, "Time field is empty");
            }
            std::tm tm = {};
            std::stringstream ss(str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (ss.fail())
            {
                throw Exception(ErrorType::INVALID_CONVERSION, "Failed to parse time: " + str);
            }
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }

        throw Exception(ErrorType::TYPE_MISMATCH, "Cannot convert field to time_point");
    }
};

template <typename T>
T extractField(const IRow& row, int field)
{
    auto fieldValue = row.getField(field);
    return FieldExtractor<T>::extract(fieldValue);
}

} // namespace casket::db