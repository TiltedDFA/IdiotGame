//
// Created by Malik T on 15/08/2025.
//
#include "Game.hpp"

namespace durak::core
{
    GameImpl::GameImpl(Config const& config,
         std::unique_ptr<Rules> rules,
         std::vector<std::unique_ptr<Player>> players):
    cfg_(config),
    rules_(std::move(rules)),
    players_(std::move(players)),
    rng_{cfg_.seed},
    hands_(players_.size())
    {
        DRK_ASSERT(players_.size() >= 2, "Less than 2 players while initalising core");
        BuildDeck();
        DRK_ASSERT(!deck_.empty(), "Empty deck after attempting init of deck in core");
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
        DRK_ASSERT(target*hands_.size() <= deck_.size(), "Less cards in deck than required to init player hands");
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

}