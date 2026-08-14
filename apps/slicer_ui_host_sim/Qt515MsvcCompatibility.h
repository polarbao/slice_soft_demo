#pragma once

#include <cstddef>

// Qt 5.15.2 引用了 MSVC 19.50 中删除的检查数组助手。
#if defined(_MSC_VER) && _MSC_VER >= 1950
namespace stdext
{
template <typename Iterator>
constexpr Iterator make_checked_array_iterator(Iterator iterator, std::size_t) noexcept
{
    return iterator;
}

template <typename Iterator>
constexpr Iterator make_unchecked_array_iterator(Iterator iterator) noexcept
{
    return iterator;
}
}
#endif
