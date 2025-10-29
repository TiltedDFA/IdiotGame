//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_ACTIONS_HPP
#define IDIOTGAME_ACTIONS_HPP

#include "Types.hpp"

namespace durak::core
{

    // player should remove and give the cards to the table so SP
    struct AttackAction   { std::vector<CardWP> cards; };
    struct DefendPair
    {
        CardWP attack;
        CardWP defend;
    };
    struct DefendAction   { std::vector<DefendPair> pairs; };
    struct TransferAction { CardWP card; };
    struct PassAction     {};
    struct TakeAction     {};

    using PlayerAction = std::variant<
      AttackAction, DefendAction, TransferAction, PassAction, TakeAction>;

    enum class MoveOutcome : uint8_t
    {
        Invalid,
        Applied,
        RoundEnded,
        GameEnded
    };

    enum class Phase : uint8_t
    {
        Attacking,
        Defending,
        Cleanup
    };
} // namespace durak::core

#endif //IDIOTGAME_ACTIONS_HPP