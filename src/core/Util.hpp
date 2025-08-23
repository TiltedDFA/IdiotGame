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
    inline auto CardToUID(Card const& c) -> uint64_t
    {
        return static_cast<uint64_t>(c.suit) * 13 + static_cast<uint64_t>(c.rank);
    }
    class CardUniqueChecker
    {
    public:
        CardUniqueChecker():
            cards_(0), contains_dup_(false) {}
        auto Add(Card const& c) -> void
        {
            uint64_t const card = uint64_t{1} << CardToUID(c);
            contains_dup_ |= static_cast<bool>(cards_ & card);
            cards_ |= card;
        }
        [[nodiscard]]
        auto ContainsDup() const -> bool
        {
            return contains_dup_;
        }
    private:
        uint64_t cards_;
        bool contains_dup_;
    };
}

#endif //IDIOTGAME_UTIL_HPP