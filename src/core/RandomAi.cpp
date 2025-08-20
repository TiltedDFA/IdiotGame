//
// Created by Malik T on 18/08/2025.
//

#include "RandomAi.hpp"
#include <random>
#include <ranges>
#include <utility>

#include "Util.hpp"

namespace durak::core
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
        auto const uncovered = std::ranges::to<std::vector<size_t>>
        (
            std::views::iota(size_t{0}, s.table.size()) |
            std::views::filter([&](size_t i)
            {
                auto const& [attack, defend] = s.table[i];
                return !attack.expired() && defend.expired();
            })
        );
        DRK_ASSERT(!uncovered.empty(), "There should be cards to defend");
        DRK_ASSERT(!util::any_invalid(std::span{s.my_hand}), "No cards in hand should be invalid");

        size_t const u_size = uncovered.size();
        size_t const h_size = s.my_hand.size();

        DRK_ASSERT(u_size <= h_size, "More attacks to cover than cards in hand breaks invariant");

        std::vector<CCardSP> attacks(u_size);
        for (size_t const idx : uncovered)
        {
            CardSP const c = s.table[idx].attack.lock();
            DRK_ASSERT(c, "Uncovered attack vanished");
            attacks.push_back(c);
        }

        std::vector<uint32_t> opt_mask(u_size, 0u);
        for (size_t k{}; k < u_size; ++k)
        {
            uint32_t mask{};
            for (size_t j{}; j < h_size; ++j)
            {
                CCardSP const c = s.table[j].defend.lock();
                bool const ok = ClassicRules::Beats(*c, *attacks[k], s.trump);
                mask |= (static_cast<uint32_t>(ok) << j);
            }
            opt_mask[k] = mask;
            //short circuit if attack card has no covering options
            if (mask == 0) return TakeAction{};
        }


        // 4) DP over defender masks to COUNT perfect matchings (full covers)
        //    dp[pos][mask] = #ways to cover attacks[pos..] using defenders in 'mask'
        uint32_t const FULL = (h_size == 32 ? 0xFFFFFFFFu : ((1u << h_size) - 1u));
        size_t const STATES = (1u << h_size);
        std::vector<uint64_t> dp((u_size + 1) * STATES, 0);
        auto D = [&](size_t pos, uint32_t mask) -> uint64_t&
        {
            return dp[pos * STATES + mask];
        };
        for (uint32_t mask = 0; mask < STATES; ++mask)
            D(u_size, mask) = 1; // base

        for (int pos = static_cast<int>(u_size) - 1; pos >= 0; --pos)
        {
            for (uint32_t mask = 0; mask < STATES; ++mask)
            {
                uint64_t sum = 0;
                uint32_t avail = opt_mask[pos] & mask;
                // iterate set bits (branch-light)
                for (uint32_t mm = avail; mm; mm &= (mm - 1))
                {
                    uint32_t bit = mm & -mm;
                    sum += D(pos + 1, mask ^ bit);
                }
                D(pos, mask) = sum;
            }
        }

        uint64_t const total = D(0, FULL);
        if (total == 0) return TakeAction{}; // no full cover exists

        // 5) Sample ONE perfect matching uniformly using the DP counts
        uint64_t r = std::uniform_int_distribution<uint64_t>(0, total - 1)(rng_);
        uint32_t mask = FULL;
        std::vector<DefendPair> pairs;
        pairs.reserve(u_size);

        for (size_t pos = 0; pos < u_size; ++pos)
        {
            uint32_t avail = opt_mask[pos] & mask;
            bool chosen = false;
            for (uint32_t mm = avail; mm; mm &= (mm - 1))
            {
                uint32_t bit = mm & -mm;
                uint64_t w = D(pos + 1, mask ^ bit);
                if (w > r)
                {
                    size_t j = std::countr_zero(bit);
                    pairs.push_back(DefendPair{
                        .attack = s.table[uncovered[pos]].attack,
                        .defend = s.my_hand[j]
                    });
                    mask ^= bit;
                    chosen = true;
                    break;
                }
                r -= w;
            }
            DRK_ASSERT(chosen, "Random sampling failed despite positive total count");
        }

        return DefendAction{ std::move(pairs) };
    }



}
