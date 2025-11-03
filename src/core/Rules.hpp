//
// Created by Malik T on 15/08/2025.
//

#ifndef IDIOTGAME_RULES_HPP
#define IDIOTGAME_RULES_HPP

#include "Actions.hpp"
#include "Types.hpp"
#include "Exception.hpp"

namespace durak::core
{
    //forward declaration
    class GameImpl;

    class Rules
    {
    public:
        using CheckResult = error::ValidateResult;

        virtual ~Rules() = default;

        // Returns unexpected(reason) for ordinary rule violations (NOT exceptions).
        // Throw only for engine misuse / broken invariants.
        virtual auto Validate(GameImpl const& game, PlayerAction const& a) const -> CheckResult = 0;

        // Mutate authoritative state (move shared_ptr<Card> hand <-> table <-> discard).
        virtual auto Apply(GameImpl& game, PlayerAction const& a) -> void = 0;

        virtual auto Advance(GameImpl& game) -> MoveOutcome = 0;
    };
}

#endif //IDIOTGAME_RULES_HPP