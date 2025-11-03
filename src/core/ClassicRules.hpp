//
// Created by Malik T on 15/08/2025.
//

#ifndef IDIOTGAME_CLASSICRULES_HPP
#define IDIOTGAME_CLASSICRULES_HPP
#include "Rules.hpp"

namespace durak::core
{
    class ClassicRules final : public Rules
    {
    public:
        auto Validate(GameImpl const& game, PlayerAction const& a) const -> CheckResult override;
        auto Apply(GameImpl& game, PlayerAction const& a) -> void override;
        auto Advance(GameImpl& game) -> MoveOutcome override;
        static bool Beats(Card const& a, Card const& b, Suit const trump);
    };
}

#endif //IDIOTGAME_CLASSICRULES_HPP