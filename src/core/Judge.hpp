//
// Created by Malik T on 26/09/2025.
//

#ifndef IDIOTGAME_JUDGE_HPP
#define IDIOTGAME_JUDGE_HPP

#include "Actions.hpp"
#include "State.hpp"
#include "Types.hpp"

namespace durak::core
{
    class GameImpl;

    enum class DesicionResult : uint8_t
    {
        OK,
        Timeout
    };

    struct TimedDecision
    {
        PlayerAction action{};
        DesicionResult result{};
    };

    class Judge
    {
    public:
        Judge() = default;

        auto GetAction(GameImpl& game, PlyrIdxT actor) const -> TimedDecision;
    };
}
#endif //IDIOTGAME_JUDGE_HPP