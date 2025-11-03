//
// RemotePlayer.cpp
//

#include "net/RemotePlayer.hpp"

#include <cstring>
#include <span>

namespace durak::net
{
    RemotePlayer::RemotePlayer(durak::core::PlyrIdxT seat,
                               std::shared_ptr<SeatChannel> chan)
        : seat_{seat}
          , chan_{std::move(chan)}
    {
    }

    void RemotePlayer::BindGame(durak::core::GameImpl& game)
    {
        game_ = &game;
    }

    auto RemotePlayer::Play(std::shared_ptr<const durak::core::GameSnapshot> snapshot,
                            std::chrono::steady_clock::time_point deadline)
        -> durak::core::PlayerAction
    {
        DRK_ASSERT(game_ != nullptr, "RemotePlayer used before BindGame()");

        std::vector<uint8_t> frame;
        bool const got = chan_->WaitPopUntil(frame, deadline);

        if (!got)
        {
            if (snapshot && snapshot->phase == durak::core::Phase::Defending)
            {
                return durak::core::TakeAction{};
            }
            return durak::core::PassAction{};
        }

        std::span<const uint8_t> u8{frame.data(), frame.size()};
        std::span<const std::byte> bytes{
            reinterpret_cast<const std::byte*>(u8.data()), u8.size()
        };

        auto const parsed =
            durak::core::net::DecodePlayerAction(*game_, bytes);

        if (!parsed.has_value())
        {
            if (snapshot && snapshot->phase == durak::core::Phase::Defending)
            {
                return durak::core::TakeAction{};
            }
            return durak::core::PassAction{};
        }

        auto const& da = parsed.value();

        // Seat spoofing guard
        if (da.actor != seat_)
        {
            if (snapshot && snapshot->phase == durak::core::Phase::Defending)
            {
                return durak::core::TakeAction{};
            }
            return durak::core::PassAction{};
        }

        return da.action;
    }
}