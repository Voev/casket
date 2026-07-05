#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>

#include <casket/db/txt/txt_database.hpp>
#include <casket/db/txt/txt_statement.hpp>
#include <casket/db/exception.hpp>

namespace casket::db
{

TxtDatabase::IndexInfo::IndexInfo()
    : hashIndex(nullptr)
    , sortedIndex(nullptr)
    , qualifier(nullptr)
    , fieldType(typeid(void))
    , isSorted(false)
{
}

TxtDatabase::IndexInfo::IndexInfo(std::type_index type, bool sorted)
    : hashIndex(nullptr)
    , sortedIndex(nullptr)
    , qualifier(nullptr)
    , fieldType(type)
    , isSorted(sorted)
{
    if (sorted)
    {
        sortedIndex = std::make_unique<SortedIndexMap>();
    }
    else
    {
        hashIndex = std::make_unique<IndexMap>();
    }
}

TxtDatabase::TxtDatabase(int fields)
    : fieldTypes_(fields, typeid(std::string))
    , errorField_(-1)
    , errorRow_(0)
{
    if (fields <= 0)
    {
        throw Exception(ErrorType::INDEX_OUT_OF_RANGE, "Fields count must be positive");
    }
    indices_.resize(fields);
}

TxtDatabase::TxtDatabase(const std::vector<std::type_index>& types)
    : fieldTypes_(types)
    , errorField_(-1)
    , errorRow_(0)
{
    if (types.empty())
    {
        throw Exception(ErrorType::INDEX_OUT_OF_RANGE, "Fields count must be positive");
    }
    indices_.resize(types.size());
}

std::shared_ptr<IFieldValue> TxtDatabase::parseField(const std::string& str, std::type_index type)
{
    if (type == typeid(std::string))
    {
        return makeFieldValue(str);
    }
    else if (type == typeid(bool))
    {
        return makeFieldValue(str == "true" || str == "1");
    }
    else if (type == typeid(char) || type == typeid(signed char))
    {
        return makeFieldValue(str.empty() ? '\0' : str[0]);
    }
    else if (type == typeid(unsigned char))
    {
        return makeFieldValue(static_cast<unsigned char>(std::stoul(str)));
    }
    else if (type == typeid(short))
    {
        return makeFieldValue(static_cast<short>(std::stoi(str)));
    }
    else if (type == typeid(unsigned short))
    {
        return makeFieldValue(static_cast<unsigned short>(std::stoul(str)));
    }
    else if (type == typeid(int))
    {
        return makeFieldValue(std::stoi(str));
    }
    else if (type == typeid(unsigned int))
    {
        return makeFieldValue(static_cast<unsigned int>(std::stoul(str)));
    }
    else if (type == typeid(long))
    {
        return makeFieldValue(std::stol(str));
    }
    else if (type == typeid(unsigned long))
    {
        return makeFieldValue(std::stoul(str));
    }
    else if (type == typeid(long long))
    {
        return makeFieldValue(std::stoll(str));
    }
    else if (type == typeid(unsigned long long))
    {
        return makeFieldValue(std::stoull(str));
    }
    else if (type == typeid(float))
    {
        return makeFieldValue(std::stof(str));
    }
    else if (type == typeid(double))
    {
        return makeFieldValue(std::stod(str));
    }
    else if (type == typeid(std::chrono::system_clock::time_point))
    {
        std::tm tm = {};
        std::stringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        return makeFieldValue(tp);
    }
    else if (type == typeid(std::vector<uint8_t>))
    {
        if (str.empty() || str == "NULL")
        {
            return makeFieldValue(std::vector<uint8_t>());
        }
        std::vector<uint8_t> data;
        data.reserve(str.length() / 2);
        for (size_t i = 0; i + 1 < str.length(); i += 2)
        {
            uint8_t byte = static_cast<uint8_t>(std::stoi(str.substr(i, 2), nullptr, 16));
            data.push_back(byte);
        }
        return makeFieldValue(data);
    }
    else
    {
        throw Exception(ErrorType::INVALID_CONVERSION, "Unsupported type for parsing: " + std::string(type.name()));
    }
}

std::string TxtDatabase::escapeString(const std::string& str) const
{
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str)
    {
        if (c == '\t' || c == '\\' || c == '\n' || c == '\r')
        {
            result += '\\';
        }
        result += c;
    }
    return result;
}

std::string TxtDatabase::unescapeString(const std::string& str) const
{
    std::string result;
    result.reserve(str.size());
    bool escape = false;
    for (char c : str)
    {
        if (escape)
        {
            result += c;
            escape = false;
        }
        else if (c == '\\')
        {
            escape = true;
        }
        else
        {
            result += c;
        }
    }
    return result;
}

void TxtDatabase::updateIndicesForRow(const TxtRow& row, size_t index, bool add)
{
    for (int i = 0; i < getNumFields(); ++i)
    {
        auto field = row.getField(i);
        if (!field)
            continue;

        if (indices_[i].hashIndex)
        {
            if (!indices_[i].qualifier || indices_[i].qualifier(row))
            {
                if (add)
                {
                    indices_[i].hashIndex->emplace(field, index);
                }
                else
                {
                    indices_[i].hashIndex->erase(field);
                }
            }
        }

        if (indices_[i].sortedIndex)
        {
            if (!indices_[i].qualifier || indices_[i].qualifier(row))
            {
                if (add)
                {
                    indices_[i].sortedIndex->emplace(field, index);
                }
                else
                {
                    indices_[i].sortedIndex->erase(field);
                }
            }
        }
    }
}

void TxtDatabase::rebuildIndices()
{
    for (auto& idx : indices_)
    {
        if (idx.hashIndex)
        {
            idx.hashIndex->clear();
        }
        if (idx.sortedIndex)
        {
            idx.sortedIndex->clear();
        }
    }

    for (size_t i = 0; i < data_.size(); ++i)
    {
        updateIndicesForRow(data_[i], i, true);
    }
}

bool TxtDatabase::validateRow(const TxtRow& row) const
{
    if (static_cast<int>(row.size()) != getNumFields())
    {
        lastError_ = "Wrong number of fields";
        return false;
    }

    for (int i = 0; i < getNumFields(); ++i)
    {
        auto field = row.getField(i);
        if (field && field->getType() != fieldTypes_[i])
        {
            lastError_ = "Type mismatch at field " + std::to_string(i);
            return false;
        }
    }

    return true;
}

bool TxtDatabase::checkIndexClashes(const TxtRow& row, size_t excludeIndex) const
{
    for (int i = 0; i < getNumFields(); ++i)
    {
        auto field = row.getField(i);
        if (!field)
            continue;

        if (indices_[i].hashIndex)
        {
            if (!indices_[i].qualifier || indices_[i].qualifier(row))
            {
                auto it = indices_[i].hashIndex->find(field);
                if (it != indices_[i].hashIndex->end() && it->second != excludeIndex)
                {
                    lastError_ = "Index clash on field " + std::to_string(i);
                    errorField_ = i;
                    errorRow_ = it->second;
                    errorRowData_ = std::make_shared<TxtRow>(data_[it->second]);
                    return false;
                }
            }
        }

        if (indices_[i].sortedIndex)
        {
            if (!indices_[i].qualifier || indices_[i].qualifier(row))
            {
                auto it = indices_[i].sortedIndex->find(field);
                if (it != indices_[i].sortedIndex->end() && it->second != excludeIndex)
                {
                    lastError_ = "Index clash on field " + std::to_string(i);
                    errorField_ = i;
                    errorRow_ = it->second;
                    errorRowData_ = std::make_shared<TxtRow>(data_[it->second]);
                    return false;
                }
            }
        }
    }

    return true;
}

bool TxtDatabase::insert(TxtRow&& row)
{
    if (!validateRow(row))
    {
        return false;
    }

    if (!checkIndexClashes(row))
    {
        return false;
    }

    size_t newIndex = data_.size();
    data_.push_back(std::move(row));
    updateIndicesForRow(data_.back(), newIndex, true);

    return true;
}

int TxtDatabase::getNumFields() const
{
    return static_cast<int>(fieldTypes_.size());
}

std::type_index TxtDatabase::getFieldType(int field) const
{
    if (field < 0 || field >= getNumFields())
    {
        return typeid(void);
    }
    return fieldTypes_[field];
}

void TxtDatabase::setFieldType(int field, std::type_index type)
{
    if (field < 0 || field >= getNumFields())
    {
        throw Exception(ErrorType::INDEX_OUT_OF_RANGE, "Field index out of range");
    }
    fieldTypes_[field] = type;
}

std::vector<std::type_index> TxtDatabase::getFieldTypes() const
{
    return fieldTypes_;
}

size_t TxtDatabase::size() const
{
    return data_.size();
}

const IRow& TxtDatabase::getRow(size_t index) const
{
    if (index >= data_.size())
    {
        throw std::out_of_range("Row index out of range");
    }
    return data_[index];
}

bool TxtDatabase::insert(std::shared_ptr<IRow> row)
{
    if (!row)
    {
        lastError_ = "Null row pointer";
        return false;
    }

    TxtRow* txtRow = dynamic_cast<TxtRow*>(row.get());
    if (txtRow)
    {
        return insert(std::move(*txtRow));
    }

    return insert(*row);
}

bool TxtDatabase::insert(const IRow& row)
{
    if (static_cast<int>(row.size()) != getNumFields())
    {
        lastError_ = "Wrong number of fields";
        return false;
    }

    TxtRow newRow(fieldTypes_);
    for (int i = 0; i < getNumFields(); ++i)
    {
        auto field = row.getField(i);
        if (field)
        {
            if (field->getType() != fieldTypes_[i])
            {
                lastError_ = "Type mismatch at field " + std::to_string(i);
                return false;
            }
            newRow.setField(i, field->clone());
        }
    }

    return insert(std::move(newRow));
}

bool TxtDatabase::insert(IRow&& row)
{
    TxtRow* txtRow = dynamic_cast<TxtRow*>(&row);
    if (txtRow)
    {
        return insert(std::move(*txtRow));
    }

    return insert(static_cast<const IRow&>(row));
}

bool TxtDatabase::update(size_t index, const IRow& newRow)
{
    if (index >= data_.size())
    {
        lastError_ = "Row index out of range";
        return false;
    }

    if (static_cast<int>(newRow.size()) != getNumFields())
    {
        lastError_ = "Wrong number of fields";
        return false;
    }

    TxtRow updatedRow(fieldTypes_);
    for (int i = 0; i < getNumFields(); ++i)
    {
        auto field = newRow.getField(i);
        if (field)
        {
            if (field->getType() != fieldTypes_[i])
            {
                lastError_ = "Type mismatch at field " + std::to_string(i);
                return false;
            }
            updatedRow.setField(i, field->clone());
        }
    }

    if (!checkIndexClashes(updatedRow, index))
    {
        return false;
    }

    updateIndicesForRow(data_[index], index, false);
    data_[index] = std::move(updatedRow);
    updateIndicesForRow(data_[index], index, true);

    return true;
}

bool TxtDatabase::remove(size_t index)
{
    if (index >= data_.size())
    {
        lastError_ = "Row index out of range";
        return false;
    }

    updateIndicesForRow(data_[index], index, false);
    data_.erase(data_.begin() + index);

    for (size_t i = index; i < data_.size(); ++i)
    {
        for (int j = 0; j < getNumFields(); ++j)
        {
            auto field = data_[i].getField(j);
            if (!field)
                continue;

            if (indices_[j].hashIndex)
            {
                if (!indices_[j].qualifier || indices_[j].qualifier(data_[i]))
                {
                    auto it = indices_[j].hashIndex->find(field);
                    if (it != indices_[j].hashIndex->end() && it->second == i + 1)
                    {
                        it->second = i;
                    }
                }
            }

            if (indices_[j].sortedIndex)
            {
                if (!indices_[j].qualifier || indices_[j].qualifier(data_[i]))
                {
                    auto it = indices_[j].sortedIndex->find(field);
                    if (it != indices_[j].sortedIndex->end() && it->second == i + 1)
                    {
                        it->second = i;
                    }
                }
            }
        }
    }

    return true;
}

void TxtDatabase::clear()
{
    data_.clear();
    for (auto& idx : indices_)
    {
        if (idx.hashIndex)
        {
            idx.hashIndex->clear();
        }
        if (idx.sortedIndex)
        {
            idx.sortedIndex->clear();
        }
    }
}

bool TxtDatabase::createIndex(int field, std::function<bool(const IRow&)> qualifier)
{
    if (field < 0 || field >= getNumFields())
    {
        lastError_ = "Index out of range";
        return false;
    }

    indices_[field] = IndexInfo(fieldTypes_[field], false);
    indices_[field].qualifier = qualifier;

    for (size_t i = 0; i < data_.size(); ++i)
    {
        const auto& row = data_[i];

        if (qualifier && !qualifier(row))
        {
            continue;
        }

        auto fieldValue = row.getField(field);
        if (!fieldValue)
        {
            continue;
        }

        auto it = indices_[field].hashIndex->find(fieldValue);
        if (it != indices_[field].hashIndex->end())
        {
            lastError_ = "Index clash at row " + std::to_string(i);
            errorField_ = field;
            errorRow_ = i;
            errorRowData_ = std::make_shared<TxtRow>(row);
            return false;
        }

        indices_[field].hashIndex->emplace(fieldValue, i);
    }

    return true;
}

bool TxtDatabase::createSortedIndex(int field, std::function<bool(const IRow&)> qualifier)
{
    if (field < 0 || field >= getNumFields())
    {
        lastError_ = "Index out of range";
        return false;
    }

    indices_[field] = IndexInfo(fieldTypes_[field], true);
    indices_[field].qualifier = qualifier;

    for (size_t i = 0; i < data_.size(); ++i)
    {
        const auto& row = data_[i];

        if (qualifier && !qualifier(row))
        {
            continue;
        }

        auto fieldValue = row.getField(field);
        if (!fieldValue)
        {
            continue;
        }

        indices_[field].sortedIndex->emplace(fieldValue, i);
    }

    return true;
}

bool TxtDatabase::dropIndex(int field)
{
    if (field < 0 || field >= getNumFields())
    {
        lastError_ = "Index out of range";
        return false;
    }

    indices_[field] = IndexInfo();
    return true;
}

bool TxtDatabase::hasIndex(int field) const
{
    if (field < 0 || field >= getNumFields())
    {
        return false;
    }
    return indices_[field].hashIndex != nullptr;
}

bool TxtDatabase::hasSortedIndex(int field) const
{
    if (field < 0 || field >= getNumFields())
    {
        return false;
    }
    return indices_[field].sortedIndex != nullptr;
}

const IRow* TxtDatabase::findByIndex(int field, std::shared_ptr<IFieldValue> value) const
{
    if (field < 0 || field >= getNumFields())
    {
        lastError_ = "Index out of range";
        return nullptr;
    }

    const auto& idx = indices_[field];
    if (!idx.hashIndex)
    {
        lastError_ = "No hash index on field " + std::to_string(field);
        return nullptr;
    }

    auto it = idx.hashIndex->find(value);
    if (it == idx.hashIndex->end())
    {
        return nullptr;
    }

    if (it->second >= data_.size())
    {
        return nullptr;
    }

    return &data_[it->second];
}

std::vector<const IRow*> TxtDatabase::findRange(int field, std::shared_ptr<IFieldValue> start,
                                                std::shared_ptr<IFieldValue> end) const
{
    std::vector<const IRow*> result;

    if (field < 0 || field >= getNumFields())
    {
        lastError_ = "Index out of range";
        return result;
    }

    const auto& idx = indices_[field];
    if (!idx.sortedIndex)
    {
        lastError_ = "No sorted index on field " + std::to_string(field);
        return result;
    }

    auto itStart = idx.sortedIndex->lower_bound(start);
    auto itEnd = idx.sortedIndex->upper_bound(end);

    for (auto it = itStart; it != itEnd; ++it)
    {
        if (it->second < data_.size())
        {
            result.push_back(&data_[it->second]);
        }
    }

    return result;
}

std::unique_ptr<IStatement> TxtDatabase::createStatement()
{
    return std::make_unique<TxtStatement>(this);
}

void TxtDatabase::write(std::ostream& out) const
{
    for (const auto& row : data_)
    {
        out << row.serialize() << '\n';
    }
}

void TxtDatabase::writeToFile(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        throw Exception(ErrorType::IO_ERROR, "Cannot open file for writing: " + filename);
    }
    write(file);
}

void TxtDatabase::read(std::istream& in)
{
    std::string line;
    int lineNum = 0;

    data_.clear();
    rebuildIndices();

    while (std::getline(in, line))
    {
        ++lineNum;

        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        TxtRow row(fieldTypes_);
        std::string field;
        bool escape = false;
        int fieldIndex = 0;

        for (char c : line)
        {
            if (escape)
            {
                field += c;
                escape = false;
            }
            else if (c == '\\')
            {
                escape = true;
            }
            else if (c == '\t')
            {
                if (fieldIndex < getNumFields())
                {
                    auto value = parseField(unescapeString(field), fieldTypes_[fieldIndex]);
                    row.setField(fieldIndex, value);
                }
                field.clear();
                ++fieldIndex;
            }
            else
            {
                field += c;
            }
        }

        if (fieldIndex < getNumFields())
        {
            auto value = parseField(unescapeString(field), fieldTypes_[fieldIndex]);
            row.setField(fieldIndex, value);
        }

        if (static_cast<int>(row.size()) != getNumFields())
        {
            throw Exception(ErrorType::WRONG_NUM_FIELDS,
                            "Line " + std::to_string(lineNum) + ": expected " + std::to_string(getNumFields()) +
                                " fields, got " + std::to_string(row.size()));
        }

        data_.push_back(std::move(row));
    }

    rebuildIndices();
}

void TxtDatabase::readFromFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw Exception(ErrorType::IO_ERROR, "Cannot open file: " + filename);
    }
    read(file);
}

std::string TxtDatabase::getLastError() const
{
    return lastError_;
}

int TxtDatabase::getErrorField() const
{
    return errorField_;
}

size_t TxtDatabase::getErrorRow() const
{
    return errorRow_;
}

std::shared_ptr<IRow> TxtDatabase::getErrorRowData() const
{
    return errorRowData_;
}

void TxtDatabase::print(std::ostream& out) const
{
    for (const auto& row : data_)
    {
        out << row.serialize() << '\n';
    }
}

bool TxtDatabase::isEmpty() const
{
    return data_.empty();
}

void TxtDatabase::reserve(size_t capacity)
{
    data_.reserve(capacity);
}

void TxtDatabase::shrinkToFit()
{
    data_.shrink_to_fit();
}

} // namespace casket::db