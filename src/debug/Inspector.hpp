//
// Created by Malik T on 19/08/2025.
//

#ifndef IDIOTGAME_INSPECTOR_HPP
#define IDIOTGAME_INSPECTOR_HPP

#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <ranges>
#include <utility>

#include "../core/Types.hpp"
#include "../core/Game.hpp"

namespace durak::core::debug
{
    struct Inspector
    {
        struct SnapshotAll
        {
            std::vector<Card const*> deck;
            std::vector<Card const*> discard;
            std::vector<std::vector<Card const*>> hands;
            std::array<std::pair<Card const*, Card const*>, constants::MaxTableSlots> table{};
            Suit trump{};
            uint8_t n_players{};
            Phase phase{};

            PlyrIdxT attacker_idx{}, defender_idx{};
            uint8_t max_deck_size{};
        };

        static inline auto Gather(GameImpl const& g) -> SnapshotAll
        {
            SnapshotAll ret{};
            ret.trump = g.trump_;
            ret.n_players = static_cast<uint8_t>(g.players_.size());
            ret.phase = g.phase_;
            ret.attacker_idx = g.attacker_idx_;
            ret.defender_idx = g.defender_idx_;
            ret.hands.resize(g.players_.size());

            ret.max_deck_size = g.cfg_.deck36 ? 36 : 52;

            for (size_t i{}; i < g.hands_.size(); ++i)
            {
                std::vector<CardSP> const& src = g.hands_[i];
                std::vector<Card const*>& dst = ret.hands[i];
                dst.reserve(src.size());
                std::ranges::transform(src, std::back_inserter(dst),
                                       [](CardSP const& c) -> Card const* { return c.get(); });
            }

            ret.deck.reserve(g.deck_.size());
            std::ranges::transform(std::as_const(g.deck_), std::back_inserter(ret.deck),
                                   [](CardSP const& c) -> Card const* { return c.get(); });

            ret.discard.reserve(g.discard_.size());
            std::ranges::transform(std::as_const(g.discard_), std::back_inserter(ret.discard),
                                   [](CardSP const& c) -> Card const* { return c.get(); });

            for (size_t i{}; i < g.table_.size(); ++i)
            {
                ret.table[i].first = g.table_[i].attack ? g.table_[i].attack.get() : nullptr;
                ret.table[i].second = g.table_[i].defend ? g.table_[i].defend.get() : nullptr;
            }

            return ret;
        }
    };
}

#endif //IDIOTGAME_INSPECTOR_HPP