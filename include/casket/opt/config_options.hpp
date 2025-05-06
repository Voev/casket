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
    template <typename SectionType>
    void add()
    {
        static_assert(std::is_base_of<Section, SectionType>::value, "SectionType must derive from Section");

        const auto& name = SectionType::name();
        if (find(name) != nullptr)
            throw std::runtime_error("Duplicated section name: " + name);

        sections_[name] = std::make_unique<SectionType>();
    }

    template <typename SectionType>
    SectionType* get() const
    {
        const auto& name = SectionType::name();
        auto section = sections_.find(name);
        if (section != sections_.end())
        {
            return dynamic_cast<SectionType*>(section->second.get());
        }
        else
        {
            throw std::runtime_error("Section not found: " + name);
        }
    }

    Section* find(const std::string& name) const
    {
        auto section = sections_.find(name);
        return section != sections_.end() ? section->second.get() : nullptr;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Section>> sections_;
};

} // namespace casket::opt
