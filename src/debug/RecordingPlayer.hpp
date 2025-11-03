//
// Created by malikt on 8/20/25.
//

#ifndef IDIOTGAME_RECORDINGPLAYER_HPP
#define IDIOTGAME_RECORDINGPLAYER_HPP

#include <memory>
#include <utility>
#include <vector>

#include "../core/Player.hpp"

namespace durak::core::debug
{
    class RecordingPlayer final : public Player
    {
    public:
        explicit RecordingPlayer(std::unique_ptr<Player> inner)
            : inner_{std::move(inner)}
        {
        }

        auto Play(std::shared_ptr<const GameSnapshot> s,
                  std::chrono::steady_clock::time_point deadline) -> PlayerAction override
        {
            last_action_ = inner_->Play(std::move(s), deadline);
            has_last_ = true;
            return last_action_;
        }

        auto HasLast() const -> bool
        {
            return has_last_;
        }

        auto Last() const -> PlayerAction const&
        {
            return last_action_;
        }

    private:
        std::unique_ptr<Player> inner_;
        PlayerAction last_action_{PassAction{}}; // harmless default
        bool has_last_{false};
    };

    // Helper to wrap a vector<unique_ptr<Player>>
    inline auto WrapRecording(std::vector<std::unique_ptr<Player>>& players)
        -> std::vector<std::unique_ptr<Player>>
    {
        std::vector<std::unique_ptr<Player>> out;
        out.reserve(players.size());

        for (auto& p : players)
        {
            out.emplace_back(std::make_unique<RecordingPlayer>(std::move(p)));
        }

        return out;
    }

    // Downcast helper (only safe if you used WrapRecording at construction)
    inline auto AsRecording(Player* p) -> RecordingPlayer*
    {
        return dynamic_cast<RecordingPlayer*>(p);
    }
} // namespace durak::core::debug

#endif //IDIOTGAME_RECORDINGPLAYER_HPP