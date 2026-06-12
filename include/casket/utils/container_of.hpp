#pragma once
#include <new>
#include <cstddef>
#include <casket/predefined/language.h>

namespace casket
{

template <typename Container, typename Member>
inline Container* container_of(Member* ptr, Member Container::* member)
{
    (void)member;

    std::ptrdiff_t offset = reinterpret_cast<std::ptrdiff_t>(&(static_cast<Container*>(nullptr)->*member));

    char* base = reinterpret_cast<char*>(ptr) - offset;

    IF_CPP17_OR_LATER(return std::launder(reinterpret_cast<Container*>(base));)
    ELSE_CPP17_OR_LATER(return reinterpret_cast<Container*>(base);)
}

} // namespace casket