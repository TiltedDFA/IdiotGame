//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_STATE_HPP
#define IDIOTGAME_STATE_HPP

#include "Types.hpp"
#include "Actions.hpp"



namespace durak::core
{
    // Immutable snapshot exposed to UI/network (non-owning)
    struct GameSnapshot
    {
        Suit trump{};
        uint8_t n_players{};
        PlyrIdxT attacker_idx{}, defender_idx{};
        Phase   phase{Phase::Attacking};

        TableViewT table{};

        // for UI: reveal my hand, counts for others
        std::vector<CardWP> my_hand;
        std::vector<uint8_t> other_counts;

        uint8_t bout_cap{};
        uint8_t attacks_used{};
        bool defender_took{false};
    };

} // namespace durak::core

#endif //IDIOTGAME_STATE_HPP