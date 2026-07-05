#include <algorithm>
#include <cctype>
#include <regex>
#include <charconv>
#include <set>

#include <casket/db/txt/txt_statement.hpp>
#include <casket/db/txt/txt_database.hpp>
#include <casket/db/txt/txt_row.hpp>
#include <casket/db/typed_field_value.hpp>

namespace casket::db
{

TxtStatement::TxtStatement(TxtDatabase* database)
    : db_(database)
    , limitOffset_(0)
    , limitCount_(0)
    , hasLimit_(false)
    , isExecuted_(false)
    , isDistinct_(false)
{
}

void TxtStatement::bind(int column, nonstd::string_view str)
{
    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = makeFieldValue(std::string(str));
    filters_.emplace_back(std::move(f));
}

void TxtStatement::bind(int column, size_t i)
{
    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = makeFieldValue(static_cast<unsigned long long>(i));
    filters_.emplace_back(std::move(f));
}

void TxtStatement::bind(int column, std::chrono::system_clock::time_point time)
{
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();

    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = makeFieldValue(static_cast<long long>(epoch));
    filters_.emplace_back(std::move(f));
}

void TxtStatement::bind(int column, const std::vector<uint8_t>& blob)
{
    std::string hex;
    hex.reserve(blob.size() * 2);
    for (uint8_t byte : blob)
    {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", byte);
        hex += buf;
    }

    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = makeFieldValue(hex);
    filters_.emplace_back(std::move(f));
}

void TxtStatement::bind(int column, const uint8_t* data, size_t len)
{
    std::vector<uint8_t> blob(data, data + len);
    bind(column, blob);
}

void TxtStatement::bindNull(int column)
{
    Filter f;
    f.type = Filter::IS_NULL;
    f.column = column;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereEquals(int column, std::shared_ptr<IFieldValue> value)
{
    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = value;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereBetween(int column, std::shared_ptr<IFieldValue> start, std::shared_ptr<IFieldValue> end)
{
    Filter f;
    f.type = Filter::BETWEEN;
    f.column = column;
    f.start = start;
    f.end = end;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::where(std::function<bool(const IRow&)> predicate)
{
    Filter f;
    f.type = Filter::CUSTOM;
    f.predicate = predicate;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereGreater(int column, std::shared_ptr<IFieldValue> value)
{
    Filter f;
    f.type = Filter::GREATER;
    f.column = column;
    f.value = value;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereLess(int column, std::shared_ptr<IFieldValue> value)
{
    Filter f;
    f.type = Filter::LESS;
    f.column = column;
    f.value = value;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereGreaterOrEqual(int column, std::shared_ptr<IFieldValue> value)
{
    Filter f;
    f.type = Filter::GREATER_OR_EQUAL;
    f.column = column;
    f.value = value;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereLessOrEqual(int column, std::shared_ptr<IFieldValue> value)
{
    Filter f;
    f.type = Filter::LESS_OR_EQUAL;
    f.column = column;
    f.value = value;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereLike(int column, const std::string& pattern, bool caseSensitive)
{
    Filter f;
    f.type = Filter::LIKE;
    f.column = column;
    f.pattern = pattern;
    f.caseSensitive = caseSensitive;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereIn(int column, const std::vector<std::shared_ptr<IFieldValue>>& values)
{
    Filter f;
    f.type = Filter::IN;
    f.column = column;
    f.values = values;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereIsNull(int column)
{
    Filter f;
    f.type = Filter::IS_NULL;
    f.column = column;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereIsNotNull(int column)
{
    Filter f;
    f.type = Filter::IS_NOT_NULL;
    f.column = column;
    filters_.emplace_back(std::move(f));
}

void TxtStatement::whereNot(int column, std::shared_ptr<IFieldValue> value)
{
    // Implement NOT EQUALS
    Filter f;
    f.type = Filter::EQUALS;
    f.column = column;
    f.value = value;
    // We'll handle NOT in matchesFilter
    filters_.emplace_back(std::move(f));
}

void TxtStatement::orderBy(int column, bool ascending)
{
    orderByClauses_.push_back({column, ascending});
}

void TxtStatement::limit(size_t offset, size_t count)
{
    limitOffset_ = offset;
    limitCount_ = count;
    hasLimit_ = true;
}

void TxtStatement::limit(size_t count)
{
    limit(0, count);
}

void TxtStatement::distinct()
{
    isDistinct_ = true;
    distinctColumns_.clear();
}

void TxtStatement::distinct(int column)
{
    isDistinct_ = true;
    distinctColumns_ = {column};
}

void TxtStatement::distinct(const std::vector<int>& columns)
{
    isDistinct_ = true;
    distinctColumns_ = columns;
}

bool TxtStatement::matchesLike(const std::string& value, const std::string& pattern, bool caseSensitive) const
{
    std::string val = caseSensitive ? value : value;
    std::string pat = caseSensitive ? pattern : pattern;

    if (!caseSensitive)
    {
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        std::transform(pat.begin(), pat.end(), pat.begin(), ::tolower);
    }

    // Convert SQL LIKE pattern to regex
    std::string regexPattern;
    regexPattern.reserve(pat.size() * 2);
    regexPattern = "^";

    for (char c : pat)
    {
        if (c == '%')
        {
            regexPattern += ".*";
        }
        else if (c == '_')
        {
            regexPattern += ".";
        }
        else if (c == '\\' || c == '.' || c == '+' || c == '*' || c == '?' || c == '(' || c == ')' || c == '[' ||
                 c == ']' || c == '{' || c == '}' || c == '^' || c == '$' || c == '|')
        {
            regexPattern += '\\';
            regexPattern += c;
        }
        else
        {
            regexPattern += c;
        }
    }

    regexPattern += "$";

    try
    {
        std::regex regex(regexPattern);
        return std::regex_match(val, regex);
    }
    catch (const std::regex_error&)
    {
        return false;
    }
}

bool TxtStatement::matchesFilter(const IRow& row, const Filter& filter) const
{
    auto field = row.getField(filter.column);

    switch (filter.type)
    {
    case Filter::EQUALS:
    {
        if (filter.value)
        {
            if (!field)
                return false;
            if (field->isNull())
                return false;
            return field->equals(*filter.value);
        }
        return !field || field->isNull();
    }

    case Filter::BETWEEN:
    {
        if (!field || field->isNull())
            return false;
        if (filter.start && field->compare(*filter.start) < 0)
            return false;
        if (filter.end && field->compare(*filter.end) > 0)
            return false;
        return true;
    }

    case Filter::GREATER:
    {
        if (!field || field->isNull() || !filter.value)
            return false;
        return field->compare(*filter.value) > 0;
    }

    case Filter::LESS:
    {
        if (!field || field->isNull() || !filter.value)
            return false;
        return field->compare(*filter.value) < 0;
    }

    case Filter::GREATER_OR_EQUAL:
    {
        if (!field || field->isNull() || !filter.value)
            return false;
        return field->compare(*filter.value) >= 0;
    }

    case Filter::LESS_OR_EQUAL:
    {
        if (!field || field->isNull() || !filter.value)
            return false;
        return field->compare(*filter.value) <= 0;
    }

    case Filter::LIKE:
    {
        if (!field || field->isNull())
            return false;
        std::string str = field->toString();
        return matchesLike(str, filter.pattern, filter.caseSensitive);
    }

    case Filter::IN:
    {
        if (!field || field->isNull())
            return false;
        for (const auto& val : filter.values)
        {
            if (field->equals(*val))
            {
                return true;
            }
        }
        return false;
    }

    case Filter::IS_NULL:
    {
        return !field || field->isNull();
    }

    case Filter::IS_NOT_NULL:
    {
        return field && !field->isNull();
    }

    case Filter::CUSTOM:
    {
        return filter.predicate(row);
    }

    default:
        return true;
    }
}

bool TxtStatement::matchesFilters(const IRow& row) const
{
    for (const auto& filter : filters_)
    {
        if (!matchesFilter(row, filter))
        {
            return false;
        }
    }
    return true;
}

void TxtStatement::applyDistinct()
{
    if (!isDistinct_)
        return;

    std::set<std::string> seen;
    std::vector<const IRow*> uniqueResults;

    for (const auto* row : results_)
    {
        std::string key;

        if (distinctColumns_.empty())
        {
            // Distinct on all columns
            key = row->serialize();
        }
        else
        {
            // Distinct on specific columns
            for (int col : distinctColumns_)
            {
                auto field = row->getField(col);
                if (field && !field->isNull())
                {
                    key += field->toString();
                }
                key += '\0';
            }
        }

        if (seen.insert(key).second)
        {
            uniqueResults.push_back(row);
        }
    }

    results_ = std::move(uniqueResults);
}

void TxtStatement::applyOrderBy()
{
    if (orderByClauses_.empty())
        return;

    std::sort(results_.begin(),
              results_.end(),
              [this](const IRow* a, const IRow* b)
              {
                  for (const auto& [field, ascending] : orderByClauses_)
                  {
                      auto valA = a->getField(field);
                      auto valB = b->getField(field);

                      // NULL handling: NULL < non-NULL
                      if (!valA || valA->isNull())
                      {
                          if (!valB || valB->isNull())
                              continue;
                          return ascending;
                      }
                      if (!valB || valB->isNull())
                      {
                          return !ascending;
                      }

                      int cmp = valA->compare(*valB);
                      if (cmp != 0)
                      {
                          return ascending ? cmp < 0 : cmp > 0;
                      }
                  }
                  return false;
              });
}

void TxtStatement::applyLimit()
{
    if (!hasLimit_)
        return;

    size_t start = std::min(limitOffset_, results_.size());
    size_t end = std::min(start + limitCount_, results_.size());

    std::vector<const IRow*> limitedResults;
    limitedResults.reserve(end - start);
    limitedResults.assign(results_.begin() + start, results_.begin() + end);
    results_ = std::move(limitedResults);
}

size_t TxtStatement::spin()
{
    results_.clear();

    // Collect all matching rows
    for (size_t i = 0; i < db_->size(); ++i)
    {
        const IRow& row = db_->getRow(i);
        if (matchesFilters(row))
        {
            results_.push_back(&row);
        }
    }

    // Apply distinct
    applyDistinct();

    // Apply order by
    applyOrderBy();

    // Apply limit
    applyLimit();

    isExecuted_ = true;
    resultIterator_ = results_.begin();
    return results_.size();
}

bool TxtStatement::step()
{
    if (!isExecuted_)
    {
        spin();
    }

    if (resultIterator_ == results_.end())
    {
        return false;
    }

    ++resultIterator_;
    return resultIterator_ != results_.end();
}

const IRow* TxtStatement::current() const
{
    if (!isExecuted_ || resultIterator_ == results_.end())
    {
        return nullptr;
    }
    return *resultIterator_;
}

nonstd::span<const uint8_t> TxtStatement::getBlob(int column)
{
    static std::vector<uint8_t> empty;

    const IRow* row = current();
    if (!row)
        return empty;

    auto field = row->getField(column);
    if (!field || field->isNull())
        return empty;

    std::string str = field->toString();
    static std::vector<uint8_t> blob;
    blob.clear();
    blob.reserve(str.length() / 2);

    for (size_t i = 0; i + 1 < str.length(); i += 2)
    {
        try
        {
            uint8_t byte = static_cast<uint8_t>(std::stoi(str.substr(i, 2), nullptr, 16));
            blob.push_back(byte);
        }
        catch (...)
        {
            return empty;
        }
    }

    return blob;
}

std::optional<std::string> TxtStatement::getString(int column)
{
    const IRow* row = current();
    if (!row)
        return std::nullopt;

    auto field = row->getField(column);
    if (!field || field->isNull())
    {
        return std::nullopt;
    }

    return field->toString();
}

size_t TxtStatement::getSizeType(int column)
{
    const IRow* row = current();
    if (!row)
        return 0;

    auto field = row->getField(column);
    if (!field || field->isNull())
        return 0;

    try
    {
        std::string str = field->toString();
        unsigned long long value;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
        if (ec == std::errc())
        {
            return static_cast<size_t>(value);
        }
        return std::stoull(str);
    }
    catch (...)
    {
        return 0;
    }
}

void TxtStatement::reset()
{
    isExecuted_ = false;
    results_.clear();
    resultIterator_ = results_.begin();
}

void TxtStatement::clear()
{
    filters_.clear();
    orderByClauses_.clear();
    hasLimit_ = false;
    isDistinct_ = false;
    distinctColumns_.clear();
    reset();
}

size_t TxtStatement::count()
{
    spin();
    return results_.size();
}

double TxtStatement::avg(int column)
{
    spin();
    if (results_.empty())
        return 0.0;

    double sum = 0.0;
    size_t count = 0;

    for (const auto* row : results_)
    {
        auto field = row->getField(column);
        if (field && !field->isNull())
        {
            try
            {
                sum += std::stod(field->toString());
                ++count;
            }
            catch (...)
            {
                // Skip invalid values
            }
        }
    }

    return count > 0 ? sum / count : 0.0;
}

double TxtStatement::sum(int column)
{
    spin();
    double total = 0.0;

    for (const auto* row : results_)
    {
        auto field = row->getField(column);
        if (field && !field->isNull())
        {
            try
            {
                total += std::stod(field->toString());
            }
            catch (...)
            {
                // Skip invalid values
            }
        }
    }

    return total;
}

double TxtStatement::min(int column)
{
    spin();
    if (results_.empty())
        return 0.0;

    double minVal = std::numeric_limits<double>::max();
    bool found = false;

    for (const auto* row : results_)
    {
        auto field = row->getField(column);
        if (field && !field->isNull())
        {
            try
            {
                double val = std::stod(field->toString());
                if (!found || val < minVal)
                {
                    minVal = val;
                    found = true;
                }
            }
            catch (...)
            {
                // Skip invalid values
            }
        }
    }

    return found ? minVal : 0.0;
}

double TxtStatement::max(int column)
{
    spin();
    if (results_.empty())
        return 0.0;

    double maxVal = std::numeric_limits<double>::lowest();
    bool found = false;

    for (const auto* row : results_)
    {
        auto field = row->getField(column);
        if (field && !field->isNull())
        {
            try
            {
                double val = std::stod(field->toString());
                if (!found || val > maxVal)
                {
                    maxVal = val;
                    found = true;
                }
            }
            catch (...)
            {
                // Skip invalid values
            }
        }
    }

    return found ? maxVal : 0.0;
}

std::vector<const IRow*> TxtStatement::getResults() const
{
    return results_;
}

bool TxtStatement::empty() const
{
    return results_.empty();
}

size_t TxtStatement::size() const
{
    return results_.size();
}

const IRow* TxtStatement::first() const
{
    return results_.empty() ? nullptr : results_.front();
}

const IRow* TxtStatement::last() const
{
    return results_.empty() ? nullptr : results_.back();
}

} // namespace casket::db