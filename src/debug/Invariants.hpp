//
// Created by Malik T on 19/08/2025.
//

#ifndef IDIOTGAME_INVARIANTS_HPP
#define IDIOTGAME_INVARIANTS_HPP

#include "../core/Game.hpp"
#include "Inspector.hpp"
#include <cassert>
#include <ranges>
#include <array>
#include <unordered_set>
#include <vector>

namespace durak::core::debug
{
    // A second layer of checks. As there is a large amount of complexity to this project,
    // being able to ensure correct state for all components is a large priority
    inline auto CheckInvariants(GameImpl const& g) -> void
    {
#if DRK_ENABLE_TEST_HOOKS == false
        (void)g;
#else
      using namespace std;

    Inspector::SnapshotAll const s = Inspector::Gather(g);

    // 1) Defend implies attack (never a defend card without an attack)
    for (auto const& slot : s.table)
    {
        bool const a = slot.first  != nullptr;
        bool const d = slot.second != nullptr;
        assert(!d || a);
    }

    // 2) In Cleanup: either table empty OR all attacks covered
    if (s.phase == Phase::Cleanup)
    {
        bool any_att = false;
        bool all_cov = true;
        for (const auto& [fst, snd] : s.table) {
            bool const a = fst  != nullptr;
            bool const d = snd != nullptr;
            any_att |= a;
            all_cov &= (!a) || d;  // a => d
        }
        assert(!any_att || all_cov);
    }

    // 3) Defender turn must have at least one uncovered attack
    if (s.phase == Phase::Defending)
    {
        bool any_uncovered = false;
        for (const auto& [fst, snd] : s.table)
        {
            bool const a = fst  != nullptr;
            bool const d = snd != nullptr;
            any_uncovered |= (a && !d);
        }
        assert(any_uncovered && "Defender turn without uncovered attacks");
    }

    // 4) Classic attack limit:
    //    total attacks on table ≤ min(table capacity, defender hand size)
    {
        size_t attacks = 0;
        for (const auto& key : s.table | views::keys)
            attacks += (key != nullptr);

        size_t const def_hand = s.hands.at(s.defender_idx).size();
        size_t const cap = std::min(constants::MaxTableSlots, def_hand);

        assert(attacks <= cap && "Attacks exceed defender capacity");
    }

    // 5) Deep: no duplicate Card* across zones + total equals deck size (36 or 52)
    {
        std::unordered_set<Card const*> seen;
        seen.reserve(s.max_deck_size);

        auto push_unique = [&](Card const* p)
        {
            if (!p) return;
            bool const inserted = seen.insert(p).second;
            assert(inserted && "Duplicate card pointer across zones");
        };

        for (auto p : s.deck)    push_unique(p);
        for (auto p : s.discard) push_unique(p);
        for (auto const& h : s.hands) for (auto const p : h) push_unique(p);
        for (auto const& t : s.table) { push_unique(t.first); push_unique(t.second); }

        assert(seen.size() == s.max_deck_size && "Materialized card count != deck size");
    }


#endif // DRK_ENABLE_TEST_HOOKS == true
    }
}
#endif //IDIOTGAME_INVARIANTS_HPP