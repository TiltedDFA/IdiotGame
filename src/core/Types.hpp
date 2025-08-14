//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_TYPES_HPP
#define IDIOTGAME_TYPES_HPP

#define DRK_ALLOW_EXCEPTIONS true

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <array>
#include <chrono>
#include <variant>
namespace durak::core::constants
{
    inline constexpr size_t MaxTableSlots = 6;
}
namespace durak::core
{
    enum class Suit : uint8_t
    {
        Hearts,
        Diamonds,
        Clubs,
        Spades
    };
    enum class Rank : uint8_t
    {
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        Ten,
        Jack,
        Queen,
        King,
        Ace
    };
    struct Card
    {
        Card() = delete;
        Card(Suit suit, Rank rank) : suit(suit), rank(rank) {}

        Suit suit;
        Rank rank;
        ///////////////////////////////////
        Card(Card const&) = delete;
        auto operator=(Card const&) -> Card& = delete;
    };
    inline auto operator==(Card const& a, Card const& b) ->bool { return a.suit == b.suit && a.rank == b.rank; }
    using CardSP = std::shared_ptr<Card>;
    using CardWP = std::weak_ptr<Card>;

    struct TableSlot
    {
        CardSP attack;
        CardSP defend;
    };
}

#endif //IDIOTGAME_TYPES_HPP