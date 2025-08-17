//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_UTIL_HPP
#define IDIOTGAME_UTIL_HPP

#include <algorithm>
#include <span>
#include <memory>
#include "OmegaException.hpp"



namespace durak::core::util
{
    template <typename T>
    inline auto any_invalid(std::span<T const> ptrs) -> bool
    {
        if constexpr (std::is_same_v<T, std::weak_ptr<typename T::element_type>>)
        {
            // For weak_ptr: check if expired
            return std::ranges::any_of(ptrs, [](auto const& p) { return p.expired(); });
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<typename T::element_type>>)
        {
            // For shared_ptr: check if null
            return std::ranges::any_of(ptrs, [](auto const& p) { return !p; });
        }
        else
        {
            static_assert([]{return false;}(), "Ptr must be std::shared_ptr<T> or std::weak_ptr<T>");
        }
    }
}

#endif //IDIOTGAME_UTIL_HPP