//
// Created by Malik T on 15/08/2025.
//

#include "ClassicRules.hpp"

#include "Game.hpp"
#include "Util.hpp"
namespace
{
    inline auto Viol(durak::core::error::RuleViolationCode code) -> durak::core::error::RuleViolation
    {
        return durak::core::error::RuleViolation{ .code = code };
    }
}

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
        using RVC = ::durak::core::error::RuleViolationCode;
        PlyrIdxT const actor = (game.phase_ == Phase::Defending) ? game.defender_idx_ : game.attacker_idx_;
        size_t const capacity_used = std::ranges::count_if(game.table_,
            [](TableSlot const& ts) {return static_cast<bool>(ts.attack);});
        size_t const capacity_free = constants::MaxTableSlots - capacity_used;
        size_t constexpr cap = constants::MaxTableSlots;
        size_t const def_hand = game.hands_[game.defender_idx_].size();

        // ::durak::core::error::RuleViolation const r_e = ::durak::core::error::RuleViolation::All;
        return std::visit([&]<typename T0>(T0 const& act) -> CheckResult
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, AttackAction>)
            {


                if (game.phase_ != Phase::Attacking)
                    return std::unexpected(Viol(RVC::WrongPhase_AttackingRequired).with_phase(game.phase_).with_actor(actor));

                if (actor != game.attacker_idx_)
                    return std::unexpected(Viol(RVC::WrongActor_AttackerRequired).with_actor(actor).with_attacker(game.attacker_idx_));

                if (act.cards.empty())
                    return std::unexpected(Viol(RVC::Attack_Empty).with_phase(game.phase_).with_actor(actor));

                if (act.cards.size() > capacity_free)
                    return std::unexpected(Viol(RVC::Attack_TooManyForCapacity)
                                           .with_actor(actor)
                                           .with_phase(game.phase_)
                                           .with_attempted(static_cast<std::uint8_t>(act.cards.size()))
                                           .with_cap_free(static_cast<std::uint8_t>(capacity_free)));

                if (util::any_invalid(std::span{act.cards}))
                    return std::unexpected(Viol(RVC::Attack_PointersInvalid).with_actor(actor));


                // size_t const after = capacity_used + ;
                // size_t const limit = std::min(cap, );

                if (act.cards.size() > def_hand)
                    return std::unexpected(Viol(RVC::Attack_TooManyForCapacity)
                                           .with_actor(actor)
                                           // .with_cap_free(static_cast<std::uint8_t>(limit - capacity_used))
                                           .with_attempted(static_cast<std::uint8_t>(act.cards.size()))
                                           .with_def_hand(static_cast<std::uint8_t>(def_hand)));
                util::CardUniqueChecker checker{};

                bool const non_empty_table = !IsEmptyAttack(game.table_);

                for (CardWP const& w : act.cards)
                {
                    CCardSP const sp = w.lock();
                    checker.Add(*sp);

                    // NEW: must be in attacker hand (by value)
                    if (game.FindFromHand(game.attacker_idx_, *sp).expired())
                        return std::unexpected(Viol(RVC::Attack_CardNotOwnedByAttacker).with_actor(actor));

                    // If table already has cards, ranks must match some rank on table
                    if (non_empty_table && !RanksMatchAnyOnTable(game.table_, sp->rank))
                        return std::unexpected(Viol(RVC::Attack_RankNotOnTableWhenRequired)
                                               .with_actor(actor).with_rank(sp->rank));
                }


                if (checker.ContainsDup())
                    return std::unexpected(Viol(RVC::Attack_DuplicateCards).with_actor(actor));
                return {};
            }
            else if constexpr (std::is_same_v<T, DefendAction>)
            {
                if (game.phase_ != Phase::Defending)
                    return std::unexpected(Viol(RVC::WrongPhase_DefendingRequired).with_phase(game.phase_).with_actor(actor));

                if (actor != game.defender_idx_)
                    return std::unexpected(Viol(RVC::WrongActor_DefenderRequired).with_actor(actor).with_defender(game.defender_idx_));

                if (act.pairs.empty())
                    return std::unexpected(Viol(RVC::Defend_Empty).with_actor(actor));

                util::CardUniqueChecker checker{};
                /*
                      *!p.attack.lock() vs !p.attack.expired() is an interesting design question
                      *From quick research !p.attack.lock() is prefered but heavier when threading
                      * as a thread could destroy the object between the expired check and the lock
                      * however for the purposes of this project, the authorative game state with power
                      * to effect object lifetime should only ever run on 1 thread so will will use
                      * the cheaper .expired() check.
                */
                for (DefendPair const& p : act.pairs)
                {
                    if (p.attack.expired() || p.defend.expired())
                        return std::unexpected(Viol(RVC::Defend_PointersInvalid).with_actor(actor));

                    CCardSP const atk = p.attack.lock();
                    CCardSP const d = p.defend.lock();

                    checker.Add(*atk);
                    checker.Add(*d);

                    // verify attack is on table and not covered
                    bool found = false;
                    for (TableSlot const& ts : game.table_)
                    {
                        if (!ts.attack) continue;
                        if (*ts.attack != *atk) continue;
                        if (static_cast<bool>(ts.defend))
                            return std::unexpected(Viol(RVC::Defend_AttackAlreadyCovered).with_actor(actor));
                        found = true;
                        break;
                    }
                    if (!found)
                        return std::unexpected(Viol(RVC::Defend_AttackNotOnTable).with_actor(actor));

                    // defender must own defend card
                    if (game.FindFromHand(game.defender_idx_, *d).expired())
                        return std::unexpected(Viol(RVC::Defend_CardNotOwnedByDefender).with_actor(actor));

                    // must beat
                    if (!Beats(*d, *atk, game.trump_))
                        return std::unexpected(Viol(RVC::Defend_DoesNotBeat).with_actor(actor));
                }

                if (checker.ContainsDup())
                    return std::unexpected(Viol(RVC::Defend_DuplicateCards).with_actor(actor));

                std::size_t const uncovered = std::ranges::count_if(
                    game.table_, [](TableSlot const& ts){ return ts.attack && !ts.defend; });

                if (uncovered != act.pairs.size())
                    return std::unexpected(Viol(RVC::Defend_UncoveredPairsMismatch)
                                           .with_actor(actor)
                                           .with_attempted(static_cast<std::uint8_t>(act.pairs.size()))
                                           .with_cap_used(static_cast<std::uint8_t>(uncovered)));

                return {};
            }
            //should only occur after defence has called a pass
            else if constexpr (std::is_same_v<T, ThrowInAction>)
            {
                if ((game.phase_ != Phase::Attacking && game.phase_ != Phase::Defending) || IsEmptyAttack(game.table_))
                    return std::unexpected(Viol(RVC::ThrowIn_WrongPhaseOrEmptyTable).with_phase(game.phase_).with_actor(actor));

                if (actor != game.attacker_idx_)
                    return std::unexpected(Viol(RVC::WrongActor_AttackerRequired).with_actor(actor).with_attacker(game.attacker_idx_));

                if (act.cards.empty())
                    return std::unexpected(Viol(RVC::ThrowIn_Empty).with_actor(actor));

                if (act.cards.size() > capacity_free)
                    return std::unexpected(Viol(RVC::ThrowIn_TooManyForCapacity)
                                           .with_actor(actor)
                                           .with_attempted(static_cast<std::uint8_t>(act.cards.size()))
                                           .with_cap_free(static_cast<std::uint8_t>(capacity_free)));

                if (util::any_invalid(std::span{act.cards}))
                    return std::unexpected(Viol(RVC::ThrowIn_PointersInvalid).with_actor(actor));

                size_t const after = capacity_used + act.cards.size();
                size_t const limit = std::min(cap, def_hand);

                if (after > limit)
                    return std::unexpected(Viol(RVC::ThrowIn_TooManyForCapacity)
                                                       .with_actor(actor)
                                                       .with_attempted(static_cast<std::uint8_t>(act.cards.size()))
                                                       .with_cap_free(static_cast<std::uint8_t>(limit - capacity_used)));

                util::CardUniqueChecker checker{};


                for (CardWP const& c : act.cards)
                {
                    CCardSP const sp = c.lock();
                    checker.Add(*sp);

                    if (game.FindFromHand(game.attacker_idx_, *sp).expired())
                        return std::unexpected(Viol(RVC::ThrowIn_CardNotOwnedByAttacker).with_actor(actor));

                    if (!RanksMatchAnyOnTable(game.table_, sp->rank))
                        return std::unexpected(Viol(RVC::ThrowIn_RankNotOnTable).with_actor(actor).with_rank(sp->rank));
                }

                if (checker.ContainsDup())
                    return std::unexpected(Viol(RVC::ThrowIn_DuplicateCards).with_actor(actor));
                return {};
            }
            else if constexpr (std::is_same_v<T, TransferAction>)
            {
                return std::unexpected(Viol(RVC::Internal_Unreachable));
            }
            else if constexpr (std::is_same_v<T, PassAction>)
            {
                if (game.phase_ != Phase::Attacking)
                    return std::unexpected(Viol(RVC::Pass_WrongPhase).with_phase(game.phase_).with_actor(actor));

                if (actor != game.attacker_idx_)
                    return std::unexpected(Viol(RVC::Pass_NotAttacker).with_actor(actor).with_attacker(game.attacker_idx_));

                if (IsEmptyAttack(game.table_))
                    return std::unexpected(Viol(RVC::Pass_TableEmpty).with_actor(actor));

                if (std::ranges::count_if(game.table_, [](TableSlot const& ts){ return ts.attack && !ts.defend; }) != 0)
                    return std::unexpected(Viol(RVC::Pass_UncoveredRemain).with_actor(actor));
                return {};
            }
            else if constexpr (std::is_same_v<T, TakeAction>)
            {
                if (game.phase_ != Phase::Defending)
                    return std::unexpected(Viol(RVC::Take_WrongPhase).with_phase(game.phase_).with_actor(actor));

                if (actor != game.defender_idx_)
                    return std::unexpected(Viol(RVC::Take_NotDefender).with_actor(actor).with_defender(game.defender_idx_));

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
                        game.MoveHandToTable(game.defender_idx_, p.attack, p.defend);
                    }
                    game.phase_ = Phase::Attacking;
                    game.defender_took_ = false;
                }
                else if constexpr(std::is_same_v<T, ThrowInAction>)
                {
                    for (CardWP const& c : act.cards)
                    {
                        game.MoveHandToTable(game.attacker_idx_, c);
                    }
                    game.phase_ = Phase::Defending;
                    game.defender_took_ = false;
                }
                else if constexpr (std::is_same_v<T, PassAction>)
                {
                    game.phase_ = Phase::Cleanup;
                }
                else if constexpr (std::is_same_v<T, TakeAction>)
                {
                    game.MoveTableToDefenderHand();
                    game.phase_ = Phase::Cleanup;
                    game.defender_took_ = true;
                }
                else if constexpr (std::is_same_v<T, TransferAction>)
                {
                    ::durak::core::error::fail(::durak::core::error::Code::Rules, "Cannot transfer in classic");
                }

            }, a);
    }

    auto ClassicRules::Advance(GameImpl& game) -> MoveOutcome
    {
        using durak::core::error::Code;
        if (game.phase_ == Phase::Attacking || game.phase_ == Phase::Defending)
            return MoveOutcome::Applied;

        //clean up phase

        if (game.defender_took_)
        {
            //defender took so cards already in hand and off table
            game.RefillHands();
            game.attacker_idx_ = game.NextLivePlayer(game.defender_idx_);
            game.defender_idx_ = game.NextLivePlayer(game.attacker_idx_);
        }
        else
        {
            if (!game.AllAttacksCovered())
                DRK_THROW(Code::State, "Cleanup reached without all attacks covered");

            game.ClearTable();

            game.RefillHands();

            game.attacker_idx_ =    game.hands_[game.defender_idx_].empty() ?
                                    game.NextLivePlayer(game.defender_idx_) :
                                    game.defender_idx_;
            game.defender_idx_ = game.NextLivePlayer(game.attacker_idx_);
        }

        game.phase_ = Phase::Attacking;
        game.defender_took_ = false;

        int with_cards = 0;
        for (auto const& h : game.hands_)
            with_cards += !h.empty();
        if (with_cards == 1) return MoveOutcome::GameEnded;

        return MoveOutcome::RoundEnded;

    }

} // durak