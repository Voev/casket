#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <casket/opt/section.hpp>

namespace casket::opt
{

class ConfigOptions final
{
public:
    ConfigOptions() = default;
    ~ConfigOptions() = default;

    ConfigOptions(const ConfigOptions&) = delete;
    ConfigOptions& operator=(const ConfigOptions&) = delete;

    template <typename SectionType>
    SectionType* add(std::string name)
    {
        static_assert(std::is_base_of<Section, SectionType>::value, "SectionType must derive from Section");

        if (find(name) != nullptr)
            throw std::runtime_error("Duplicated section name: " + name);

        auto section = std::make_unique<SectionType>();
        auto* raw = section.get();

        sections_.emplace(std::move(name), std::move(section));
        return raw;
    }

    template <typename SectionType>
    SectionType* add()
    {
        static_assert(std::is_base_of<Section, SectionType>::value, "SectionType must derive from Section");
        return add<SectionType>(SectionType::name());
    }

    template <typename SectionType>
    SectionType* get() const
    {
        return get<SectionType>(SectionType::name());
    }

    template <typename SectionType>
    SectionType* get(const std::string& name) const
    {
        auto* sec = find(name);
        if (sec == nullptr)
            throw std::runtime_error("Section not found: " + name);

        auto* typed = dynamic_cast<SectionType*>(sec);
        if (typed == nullptr)
            throw std::runtime_error("Section type mismatch: " + name);

        return typed;
    }

    Section* find(const std::string& name) const
    {
        auto it = sections_.find(name);
        return it != sections_.end() ? it->second.get() : nullptr;
    }

    bool remove(const std::string& name)
    {
        return sections_.erase(name) > 0;
    }

    template <typename SectionType>
    bool remove()
    {
        return remove(SectionType::name());
    }

    template <typename Fn>
    void forEach(Fn&& fn) const
    {
        for (const auto& [name, sec] : sections_)
        {
            fn(name, *sec);
        }
    }

    std::size_t size() const noexcept
    {
        return sections_.size();
    }

    bool empty() const noexcept
    {
        return sections_.empty();
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Section>> sections_;
};

} // namespace casket::opt
