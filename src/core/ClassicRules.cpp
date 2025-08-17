//
// Created by Malik T on 15/08/2025.
//

#include "ClassicRules.hpp"

#include "Game.hpp"
#include "Util.hpp"

namespace durak::core
{
    auto ClassicRules::Beats(Card const& a, Card const& b, Suit const trump) -> bool
    {
        if (a.suit == b.suit) return a.rank > b.rank;
        return a.suit == trump && b.suit != trump;
    }
    static auto IsEmptyAttack(TableT const& t) -> bool
    {
        for (TableSlot const& ts : t) if (ts.attack) return false;
        return true;
    }
    static auto RanksMatchAnyOnTable(TableT const& t, Rank const r) -> bool
    {
        for (TableSlot const& ts : t )
        {
            if (ts.attack && ts.attack->rank == r) return true;
            if (ts.defend && ts.defend->rank == r) return true;
        }
        return false;
    }
    auto ClassicRules::Validate(GameImpl const& game, PlayerAction const& a) const -> CheckResult
    {
        PlyrIdxT const actor = (game.phase_ == Phase::Defending) ? game.defender_idx_ : game.attacker_idx_;
        size_t const capacity_used = std::ranges::count_if(game.table_,
            [](TableSlot const& ts) {return static_cast<bool>(ts.attack);});
        size_t const capacity_free = constants::MaxTableSlots - capacity_used;

        ::durak::core::error::RuleViolation const r_e = ::durak::core::error::RuleViolation::All;
        return std::visit([&]<typename T0>(T0 const& act) -> CheckResult
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, AttackAction>)
            {
                if (game.phase_ != Phase::Attacking)
                    return std::unexpected(r_e);
                if (actor != game.attacker_idx_)
                    return std::unexpected(r_e);
                if (act.cards.empty())
                    return std::unexpected(r_e);
                if (act.cards.size() > capacity_free)
                    return std::unexpected(r_e);
                if (util::any_invalid(std::span{act.cards}))
                    return std::unexpected(r_e);
                if (!IsEmptyAttack(game.table_))
                {
                    for (CardWP const& c : act.cards)
                    {
                        if (!RanksMatchAnyOnTable(game.table_, c.lock()->rank))
                            return std::unexpected(r_e);
                    }
                }
                return {};
            }
            else if constexpr (std::is_same_v<T, DefendAction>)
            {
                if (game.phase_ != Phase::Defending)
                    return std::unexpected(r_e);
                if (actor != game.defender_idx_)
                    return std::unexpected(r_e);
                if (act.pairs.empty())
                    return std::unexpected(r_e);

                for (DefendPair const& p : act.pairs)
                {
                    if (!Beats(*p.defend.lock(), *p.attack.lock(), game.trump_))
                        return std::unexpected(r_e);
                    bool found = false;
                    for (TableSlot const& ts : game.table_)
                    {
                        if (!ts.attack) continue;
                        if (*ts.attack != *p.attack.lock()) continue;
                        if (static_cast<bool>(ts.defend))
                            return std::unexpected(r_e);
                        found = true;
                        break;
                    }
                    if (!found) return std::unexpected(r_e);
                }

                return {};
            }
            //should only occur after defence has called a pass
            else if constexpr (std::is_same_v<T, ThrowInAction>)
            {
                if (game.phase_ != Phase::Attacking && game.phase_ != Phase::Defending)
                    return std::unexpected(r_e);
                if (actor != game.attacker_idx_)
                    return std::unexpected(r_e);
                if (act.cards.empty())
                    return std::unexpected(r_e);
                if (act.cards.size() > capacity_free)
                    return std::unexpected(r_e);
                if (IsEmptyAttack(game.table_))
                    return std::unexpected(r_e);
                for (CardWP const& c : act.cards)
                {
                    if (!RanksMatchAnyOnTable(game.table_, c.lock()->rank))
                        return std::unexpected(r_e);
                }
                return {};
            }
            else if constexpr (std::is_same_v<T, TransferAction>)
            {
                return std::unexpected(r_e);
            }
            else if constexpr (std::is_same_v<T, PassAction>)
            {
                if (game.phase_ != Phase::Attacking)
                    return std::unexpected(r_e);
                if (actor != game.attacker_idx_)
                    return std::unexpected(r_e);
                return {};
            }
            else if constexpr (std::is_same_v<T, TakeAction>)
            {
                if (game.phase_ != Phase::Defending)
                    return std::unexpected(r_e);
                if (actor != game.defender_idx_)
                    return std::unexpected(r_e);
                return {};
            }
            DRK_THROW(durak::core::error::Code::Unknown, "Should never reach this point");
        }, a);
    }

    auto ClassicRules::Apply(GameImpl& game, PlayerAction const& a) -> void
    {
        std::visit([&]<typename T0>(T0 const& act)
            {
                using T = std::decay_t<T0>;
                if constexpr(std::is_same_v<T, AttackAction>)
                {
                    for (CardWP const& c : act.cards)
                    {
                        game.MoveHandToTable(game.attacker_idx_, c);
                    }
                    game.phase_ = Phase::Defending;
                    game.defender_took_ = false;
                }
                else if constexpr(std::is_same_v<T, DefendAction>)
                {
                    for (DefendPair const& p : act.pairs)
                    {
                        game.move
                    }
                }
            }, a);
    }

    auto ClassicRules::Advance(GameImpl& game) -> MoveOutcome
    {

    }

} // durak