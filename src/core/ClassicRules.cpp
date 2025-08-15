//
// Created by Malik T on 15/08/2025.
//

#include "ClassicRules.hpp"

#include "Game.hpp"

namespace durak::core
{
    bool ClassicRules::Beats(Card const& a, Card const& b, Suit const trump)
    {
        if (a.suit == b.suit) return a.rank > b.rank;
        return a.suit == trump && b.suit != trump;
    }
    auto ClassicRules::Validate(GameImpl const& game, PlayerAction const& a) const -> CheckResult
    {
        // uint8_t const active_player = game.
    }

    auto ClassicRules::Apply(GameImpl& game, PlayerAction const& a) -> void
    {

    }

    auto ClassicRules::Advance(GameImpl& game) -> MoveOutcome
    {

    }

} // durak