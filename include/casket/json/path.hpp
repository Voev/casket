#pragma once

#include <string>
#include <array>
#include <algorithm>
#include <casket/nonstd/string_view.hpp>

namespace casket::json
{

class Path final
{
public:
    Path() = default;

    Path(nonstd::string_view path)
    {
        parse(path);
    }

    Path(const char* path)
    {
        parse(nonstd::string_view(path));
    }

    template <size_t N>
    Path(const std::array<nonstd::string_view, N>& segments)
    {
        for (const auto& seg : segments)
        {
            parts_[count_++] = seg;
        }
    }

    Path(std::initializer_list<nonstd::string_view> segments)
    {
        count_ = std::min(segments.size(), MAX_DEPTH);
        size_t i = 0;
        for (auto seg : segments)
        {
            if (i >= MAX_DEPTH)
                break;
            parts_[i++] = seg;
        }
    }

    Path& operator/=(nonstd::string_view segment)
    {
        if (count_ < MAX_DEPTH)
        {
            parts_[count_++] = segment;
        }
        return *this;
    }

    Path operator/(nonstd::string_view segment) const
    {
        Path result = *this;
        result /= segment;
        return result;
    }

    bool empty() const noexcept
    {
        return count_ == 0;
    }
    size_t depth() const noexcept
    {
        return count_;
    }
    nonstd::string_view operator[](size_t index) const
    {
        return parts_[index];
    }

    std::string toString() const
    {
        std::string result;
        for (size_t i = 0; i < count_; ++i)
        {
            if (i > 0)
                result += '.';
            result += parts_[i];
        }
        return result;
    }

    nonstd::string_view front() const
    {
        return parts_[0];
    }
    nonstd::string_view back() const
    {
        return parts_[count_ - 1];
    }

    Path parent() const
    {
        if (count_ == 0)
            return Path{};
        Path result;
        result.count_ = count_ - 1;
        for (size_t i = 0; i < result.count_; ++i)
        {
            result.parts_[i] = parts_[i];
        }
        return result;
    }

    nonstd::string_view leaf() const
    {
        return count_ == 0 ? nonstd::string_view{} : parts_[count_ - 1];
    }

    auto begin() const
    {
        return parts_.begin();
    }
    auto end() const
    {
        return parts_.begin() + count_;
    }

    bool operator==(const Path& other) const
    {
        if (count_ != other.count_)
            return false;
        for (size_t i = 0; i < count_; ++i)
        {
            if (parts_[i] != other.parts_[i])
                return false;
        }
        return true;
    }

    bool operator!=(const Path& other) const
    {
        return !(*this == other);
    }

private:
    static constexpr size_t MAX_DEPTH = 16;

    void parse(nonstd::string_view path)
    {
        size_t start = 0;
        size_t end = path.find('.');

        while (end != nonstd::string_view::npos && count_ < MAX_DEPTH)
        {
            parts_[count_++] = path.substr(start, end - start);
            start = end + 1;
            end = path.find('.', start);
        }
        if (count_ < MAX_DEPTH && start < path.size())
        {
            parts_[count_++] = path.substr(start);
        }
    }

    std::array<nonstd::string_view, MAX_DEPTH> parts_{};
    size_t count_ = 0;
};

inline Path operator/(nonstd::string_view left, nonstd::string_view right)
{
    Path p(left);
    p /= right;
    return p;
}

} // namespace casket::json