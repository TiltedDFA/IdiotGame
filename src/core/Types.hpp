//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_TYPES_HPP
#define IDIOTGAME_TYPES_HPP

#define DRK_ALLOW_EXCEPTIONS true
#define DRK_ENABLE_TEST_HOOKS true

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <array>
#include <chrono>
#include <random>
#include <variant>
namespace durak::core::constants
{
    inline constexpr size_t MaxTableSlots = 6;
}
namespace durak::core
{
    enum class Suit : uint8_t
    {
        Hearts = 0,
        Diamonds,
        Clubs,
        Spades
    };
    enum class Rank : uint8_t
    {
        Two = 0,
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
    inline auto operator==(Card const& a, Card const& b) -> bool { return a.suit == b.suit && a.rank == b.rank; }
    using CardSP = std::shared_ptr<Card>;
    using CCardSP = std::shared_ptr<Card const>;
    using CardWP = std::weak_ptr<Card>;
    using CCardWP = std::weak_ptr<Card const>;

    struct TableSlot
    {
        CardSP attack;
        CardSP defend;
    };

    struct TableSlotView
    {
        CardWP attack;
        CardWP defend;
    };


    using TableT = std::array<TableSlot, constants::MaxTableSlots>;
    using TableViewT = std::array<TableSlotView, constants::MaxTableSlots>;
    struct Config
    {
        uint32_t n_players{2};
        uint8_t  deal_up_to{6};
        // true = 36-card (Six..Ace), false = 52-card
        bool     deck36{true};
        uint64_t seed{std::random_device{}()};
        std::chrono::milliseconds turn_timeout{std::chrono::seconds(30ULL)};
    };
    using PlyrIdxT = uint8_t;
}

#endif //IDIOTGAME_TYPES_HPP