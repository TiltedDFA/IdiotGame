//
// Created by Malik T on 15/08/2025.
//

#ifndef IDIOTGAME_GAME_HPP
#define IDIOTGAME_GAME_HPP

#include <algorithm>
#include <random>
#include <span>
#include <string>
#include "Types.hpp"
#include "Actions.hpp"
#include "State.hpp"
#include "Rules.hpp"
#include "Player.hpp"

namespace durak::core::debug {struct Inspector;}
namespace durak::core
{
    //forward declare
    class GameImpl
    {
    public:
        GameImpl() = delete;
        GameImpl(Config const& config,
                 std::unique_ptr<Rules> rules,
                 std::vector<std::unique_ptr<Player>> players);

        // One state-machine step: ask current actor for an action, validate/apply/advance.
        auto Step() -> MoveOutcome;
        auto SnapshotFor(uint8_t seat) const -> std::shared_ptr<GameSnapshot const>;

        auto Attacker() const noexcept      -> PlyrIdxT { return attacker_idx_; }
        auto Defender() const noexcept      -> PlyrIdxT { return defender_idx_; }
        auto PhaseNow() const noexcept      -> Phase   { return phase_;  }
        auto Trump()    const noexcept      -> Suit    { return trump_;  }
        auto PlayerCount() const noexcept   -> size_t { return players_.size(); }

        //allows class to directly access private data on an instance
        friend class ClassicRules;
        friend struct debug::Inspector;

        //returns nullptr if doesnt exist
        auto FindFromHand(PlyrIdxT const seat, Card const& c) const -> CardWP;
        auto FindFromAtkTable(Card const& c) const -> CardWP;

        //Handles both moving cards to attk and defend, will treat intent as move to atk
        //if def is null. Throws if invariants break.
        auto MoveHandToTable(PlyrIdxT const seat, CardWP const& atk, CardWP const& def = {}) -> void;
        auto ClearTable() -> void;
        auto MoveTableToDefenderHand() -> void;
        //Uses the specific order for Durak
        auto RefillHands() -> void;

        auto NextLivePlayer(PlyrIdxT from) const -> PlyrIdxT;

        inline auto NextSeat(PlyrIdxT const idx) const -> PlyrIdxT { return static_cast<PlyrIdxT>((idx + 1) % players_.size()); }
        auto AllAttacksCovered() const -> bool;
        auto PlayerAt(PlyrIdxT seat) -> Player* { return players_[seat].get(); }
    private:
        //Produces a shuffled deck
        auto BuildDeck() -> void;
        auto DealInitalHands() -> void;
        auto ChoseInitalRoles() -> void;
    private:
        Config cfg_;
        std::unique_ptr<Rules> rules_;
        std::vector<std::unique_ptr<Player>> players_;
        std::mt19937_64 rng_;

        // Authoritative state
        std::vector<std::vector<CardSP>> hands_;             // [seat] owns cards in hand
        std::array<TableSlot, constants::MaxTableSlots> table_{};      // owns table cards
        std::vector<CardSP> deck_;                           // owns remaining deck cards
        std::vector<CardSP> discard_;                        // beaten cards

        // Turn/round state
        Suit    trump_{Suit::Spades};
        uint8_t attacker_idx_{0}, defender_idx_{1};
        Phase   phase_{Phase::Attacking};
        bool    defender_took_{false}; // set by Apply(Take)
    };
}
#endif //IDIOTGAME_GAME_HPP