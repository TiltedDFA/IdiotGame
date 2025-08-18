//
// Created by Malik T on 18/08/2025.
//

#include "RandomAi.hpp"
#include <random>
#include <ranges>
#include <utility>

#include "../core/Util.hpp"

namespace durak::test
{
    RandomAI::RandomAI(uint64_t rng_seed):
        rng_(rng_seed) {}

    using namespace durak::core;

    auto RandomAI::Play(std::shared_ptr<const GameSnapshot> snapshot, std::chrono::steady_clock::time_point deadline)
        -> durak::core::PlayerAction
    {
        (void)deadline;

        if (snapshot->phase == Phase::Attacking)
        {
            return AttackMove(*snapshot);
        }
        if (snapshot->phase == Phase::Defending)
        {
            return DefendMove(*snapshot);
        }
        return PassAction{};
    }

    static inline auto Beats(Card const& d, Card const& a, Suit const trump) -> bool
    {
        return ClassicRules::Beats(d, a, trump);
    }

    auto RandomAI::AttackMove(GameSnapshot const& s) -> PlayerAction
    {
        DRK_ASSERT(!durak::core::util::any_invalid(std::span{s.my_hand}), "No cards in hand should be invalid");
        bool empty = true;
        for (TableSlotView const& w : s.table)
        {
            empty &= w.attack.expired();
        }

        if (s.my_hand.empty()) return PassAction{};

        if (empty)
        {
            return AttackAction{std::vector{s.my_hand[pick(s.my_hand)]}};
        }


        constexpr size_t RANK_COUNT = std::to_underlying(durak::core::Rank::Ace) + 1;
        std::array<uint8_t, RANK_COUNT> counts{};
        for (TableSlotView const& ts : s.table)
        {
            if (CardSP const a = ts.attack.lock()) counts[std::to_underlying(a->rank)] = 1;
            if (CardSP const a = ts.defend.lock()) counts[std::to_underlying(a->rank)] = 1;
        }

        auto cand_view = s.my_hand | std::views::filter([&](CardWP const& c)
        {
            return counts[std::to_underlying(c.lock()->rank)] != 0;
        });

        std::vector<CardWP> cand = std::ranges::to<std::vector<CardWP>>(cand_view);

        if (cand.empty()) return PassAction{};

        return AttackAction{std::vector{cand[pick(cand)]}};
    }

    auto RandomAI::DefendMove(GameSnapshot const& s) -> PlayerAction
    {
        // I think theres a function for this in game, check.
        auto const slot_it = std::ranges::find_if(s.table,
            [](TableSlotView const& ts)
            {
              return true;
            }
            );
    }



}
