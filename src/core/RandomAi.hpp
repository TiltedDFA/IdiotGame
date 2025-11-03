//
// Created by Malik T on 18/08/2025.
//

#ifndef IDIOTGAME_RANDOMAI_HPP
#define IDIOTGAME_RANDOMAI_HPP

#include "Player.hpp"
#include "ClassicRules.hpp"
#include "State.hpp"
#include "Types.hpp"

namespace durak::core
{
    class RandomAI final : public durak::core::Player
    {
    public:
        explicit RandomAI(uint64_t rng_seed);

        auto Play(std::shared_ptr<const durak::core::GameSnapshot> snapshot,
                  std::chrono::steady_clock::time_point deadline) -> durak::core::PlayerAction override;

    private:
        template <class Vec>
        auto pick(Vec const& v) -> size_t
        {
            return std::uniform_int_distribution<size_t>{0, v.size() - 1}(rng_);
        }

        auto AttackMove(durak::core::GameSnapshot const&) -> durak::core::PlayerAction;
        auto DefendMove(durak::core::GameSnapshot const&) -> durak::core::PlayerAction;

    private:
        std::mt19937 rng_;
    };
}

#endif //IDIOTGAME_RANDOMAI_HPP