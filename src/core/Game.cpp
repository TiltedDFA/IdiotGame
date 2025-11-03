//
// Created by Malik T on 15/08/2025.
//
#include "Game.hpp"
#include <ranges>

#include "Util.hpp"
#include <print>
#include <utility>

namespace durak::core
{
    GameImpl::GameImpl(Config const& config,
                       std::unique_ptr<Rules> rules,
                       std::vector<std::unique_ptr<Player>> players) :
        cfg_(config),
        rules_(std::move(rules)),
        players_(std::move(players)),
        rng_{cfg_.seed},
        judge_(std::make_shared<Judge>()),
        hands_(players_.size())
    {
        DRK_ASSERT(players_.size() >= 2, "Less than 2 players while initalising core");
        DRK_ASSERT(!std::ranges::any_of(players_,
                                        [](std::unique_ptr<Player> const& p) { return !p; }), "Invalid player in core");
        BuildDeck();
        DRK_ASSERT(!deck_.empty(), "Empty deck after attempting init of deck in core");
        trump_ = deck_.back()->suit;
        DealInitalHands();
        ChoseInitalRoles();
    }

    auto GameImpl::BuildDeck() -> void
    {
        deck_.clear();
        //branchless init
        size_t const rank_start = cfg_.deck36 * static_cast<size_t>(Rank::Six);
        constexpr size_t rank_end = static_cast<size_t>(Rank::Ace) + 1;
        for (size_t i{}; i < 4; ++i)
        {
            for (size_t j{rank_start}; j < rank_end; ++j)
            {
                deck_.emplace_back(std::make_shared<Card>(
                    static_cast<Suit>(i), static_cast<Rank>(j)));
            }
        }
        std::ranges::shuffle(deck_, rng_);
    }

    auto GameImpl::DealInitalHands() -> void
    {
        size_t target = cfg_.deal_up_to;
        DRK_ASSERT(target * hands_.size() <= deck_.size(), "Less cards in deck than required to init player hands");
        //will not deal round robin as with a randomly shuffled deck
        //dealing order should not matter.
        for (auto& hand : hands_)
        {
            while (hand.size() < target)
            {
                hand.push_back(std::move(deck_.back()));
                deck_.pop_back();
            }
        }
    }

    auto GameImpl::ChoseInitalRoles() -> void
    {
        attacker_idx_ = 0;
        defender_idx_ = NextSeat(attacker_idx_);
        phase_ = Phase::Attacking;
        defender_took_ = false;
    }

    //helpers for snapshotfor
    template <typename T>
    static auto Shared_to_weak(std::vector<std::shared_ptr<T>> const& vec)
        -> std::vector<std::weak_ptr<T>>
    {
        return std::vector<std::weak_ptr<T>>(vec.cbegin(), vec.cend());
    }

    template <std::size_t... I>
    inline auto ViewTableHelper(TableT const& src, std::index_sequence<I...>) -> TableViewT
    {
        return TableViewT{TableSlotView{src[I].attack, src[I].defend}...};
    }

    inline auto MakeViewTable(TableT const& src) -> TableViewT
    {
        return ViewTableHelper(src, std::make_index_sequence<constants::MaxTableSlots>{});
    }

    auto GameImpl::SnapshotFor(uint8_t seat) const -> std::shared_ptr<GameSnapshot const>
    {
        std::shared_ptr<GameSnapshot> snap = std::make_shared<GameSnapshot>();
        snap->trump = trump_;
        snap->n_players = players_.size();
        snap->attacker_idx = attacker_idx_;
        snap->defender_idx = defender_idx_;
        snap->phase = phase_;

        // snap->table = std::move(MakeViewTable(table_));
        snap->table = MakeViewTable(table_);
        snap->my_hand = std::move(Shared_to_weak(hands_[seat]));

        for (auto const& hand : hands_)
        {
            snap->other_counts.push_back(hand.size());
        }
        auto used = static_cast<uint8_t>(
            std::ranges::count_if(table_, [](auto const& ts) { return static_cast<bool>(ts.attack); })
        );
        snap->bout_cap = bout_cap_;
        snap->attacks_used = used;
        snap->defender_took = defender_took_;

        return snap;
    }

    auto GameImpl::FindFromHand(PlyrIdxT const seat, Card const& c) const -> CardWP
    {
        auto const it = std::ranges::find_if(std::as_const(hands_[seat]),
                                             [&c](CardSP const& csp) { return c == *csp; });
        return (it != std::cend(hands_[seat])) ? CardWP{*it} : CardWP{};
    }

    auto GameImpl::FindFromAtkTable(Card const& c) const -> CardWP
    {
        auto const it = std::ranges::find_if(std::as_const(table_),
                                             [&c](TableSlot const& ts)
                                             {
                                                 if (!ts.attack) return false;
                                                 return c == *ts.attack;
                                             });
        return (it != std::cend(table_)) ? CardWP{it->attack} : CardWP{};
    }

    auto GameImpl::MoveHandToTable(PlyrIdxT const seat, CardWP const& atk, CardWP const& def) -> void
    {
        DRK_ASSERT(!atk.expired(), "Attacker card null (Should never happen)");

        CCardSP atk_card = atk.lock();
        auto& hand = hands_.at(seat);
        //if defender card not present, the intended request is interpreted as an attacker
        //conducting an attack
        if (def.expired())
        {
            auto const it =
                std::ranges::find_if(hand, [&atk_card](CardSP const& csp)
                {
                    return *csp == *atk_card;
                });
            DRK_ASSERT(it != std::end(hand), "Attacker card not in hand");

            auto const free_slot_it = std::ranges::find_if(table_,
                                                           [](TableSlot const& s) { return !(s.attack); });

            if (free_slot_it == std::end(table_))
                DRK_THROW(durak::core::error::Code::State, "No free table slots");

            free_slot_it->attack = std::move(*it);
            hand.erase(it);
        }
        else
        {
            CCardSP def_card = def.lock();
            auto const it =
                std::ranges::find_if(hand, [&def_card](CardSP const& csp)
                {
                    return *csp == *def_card;
                });

            DRK_ASSERT(it != std::end(hand), "Defender card not in hand");

            auto const cover_slot_it = std::ranges::find_if(table_,
                                                            [&](TableSlot const& s)
                                                            {
                                                                return s.attack && *s.attack == *atk_card;
                                                            });

            if (cover_slot_it == std::end(table_)) DRK_THROW(durak::core::error::Code::State,
                                                             "Card which you attempt to cover doesn't exist");
            if (cover_slot_it->defend) DRK_THROW(durak::core::error::Code::State,
                                                 "Card which you attempt to cover is already covered");

            cover_slot_it->defend = std::move(*it);
            hand.erase(it);
        }
    }

    auto GameImpl::ClearTable() -> void
    {
        // auto reset_table_slot = [](TableSlot& ts) {ts.attack.reset(); ts.defend.reset();};
        // std::ranges::for_each(table_, reset_table_slot);

        for (auto& [attack, defend] : table_)
        {
            if (attack) discard_.push_back(std::move(attack));
            if (defend) discard_.push_back(std::move(defend));
            attack.reset();
            defend.reset();
        }
    }

    auto GameImpl::MoveTableToDefenderHand() -> void
    {
        auto& hand = hands_.at(defender_idx_);
        for (auto& ts : table_)
        {
            if (ts.attack) hand.push_back(std::move(ts.attack));
            if (ts.defend) hand.push_back(std::move(ts.defend));
            ts.attack.reset();
            ts.defend.reset();
        }
    }

    auto GameImpl::RefillHands() -> void
    {
        auto needs_cards = [&](PlyrIdxT const seat) { return hands_[seat].size() < cfg_.deal_up_to; };
        auto draw_card = [&](PlyrIdxT const seat) -> bool
        {
            if (deck_.empty()) return false;
            hands_[seat].push_back(std::move(deck_.back()));
            deck_.pop_back();
            return true;
        };

        bool was_drawn = true;
        while (was_drawn)
        {
            was_drawn = false;
            for (uint8_t offset = 0; offset < players_.size(); ++offset)
            {
                uint8_t const seat = (attacker_idx_ + offset) % players_.size();
                if (needs_cards(seat)) was_drawn |= draw_card(seat);
                if (deck_.empty()) break;
            }
        }
    }

    auto GameImpl::NextLivePlayer(PlyrIdxT const from) const -> PlyrIdxT
    {
        PlyrIdxT i{from};
        size_t const n = players_.size();
        for (size_t j{}; j < n; ++j)
        {
            i = NextSeat(i);
            if (!hands_[i].empty()) return i;
        }
        DRK_THROW(durak::core::error::Code::State, "No live players");
    }

    auto GameImpl::AllAttacksCovered() const -> bool
    {
        return std::ranges::none_of(table_,
                                    [](TableSlot const& ts) { return ts.attack && !ts.defend; });
    }

    auto GameImpl::Step() -> MoveOutcome
    {
        PlyrIdxT const actor = (phase_ == Phase::Defending) ? defender_idx_ : attacker_idx_;

        // std::shared_ptr<GameSnapshot const> snap{SnapshotFor(actor)};
        // auto const deadline = std::chrono::steady_clock::now() + cfg_.turn_timeout;
        //
        // PlayerAction const action = players_[actor]->Play(std::move(snap), deadline);
        TimedDecision const dec = judge_->GetAction(*this, actor);
        PlayerAction const action = dec.action;

        if (auto const ok = rules_->Validate(*this, action); !ok.has_value())
        {
            std::print("{}\n", durak::core::error::describe(ok.error()));
            return MoveOutcome::Invalid;
        }
        rules_->Apply(*this, action);
        return rules_->Advance(*this);;
    }
}