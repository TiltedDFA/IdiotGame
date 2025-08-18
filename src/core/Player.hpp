//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_PLAYER_HPP
#define IDIOTGAME_PLAYER_HPP

#include "Actions.hpp"
#include "State.hpp"

namespace durak::core
{
    class Player
    {
    public:
        virtual ~Player() = default;

        // Called by the authoritative game loop (local AI/human adapter or server-side remote).
        // Deadline is authoritative; on timeout the caller will default (Pass/Take).
        virtual PlayerAction Play(std::shared_ptr<const GameSnapshot> snapshot,
                                  std::chrono::steady_clock::time_point deadline) = 0;
    };
}
#endif //IDIOTGAME_PLAYER_HPP