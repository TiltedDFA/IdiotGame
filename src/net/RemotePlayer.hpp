//
// RemotePlayer.hpp — server-side network Player adapter using WebSocket++
//

#ifndef IDIOTGAME_REMOTEPLAYER_HPP
#define IDIOTGAME_REMOTEPLAYER_HPP

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <chrono>
#include <span>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "core/Player.hpp"
#include "core/Types.hpp"
#include "core/State.hpp"
#include "core/Actions.hpp"
#include "core/Game.hpp"
#include "core/Exception.hpp"

#include "net/codec.hpp" // BuildSnapshot / DecodePlayerAction

namespace durak::net
{
    using WsServer = websocketpp::server<websocketpp::config::asio>;
    using Hdl      = websocketpp::connection_hdl;

    struct SeatChannel
    {
        std::weak_ptr<WsServer>          ep;
        Hdl                              hdl;

        std::mutex                       mtx;
        std::condition_variable          cv;
        std::deque<std::vector<uint8_t>> inbox;
        bool                             connected{false};

        void Enqueue(std::vector<uint8_t> bytes)
        {
            {
                std::lock_guard<std::mutex> lock(mtx);
                inbox.emplace_back(std::move(bytes));
            }
            cv.notify_all();
        }

        bool WaitPopUntil(std::vector<uint8_t>& out,
                          std::chrono::steady_clock::time_point deadline)
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait_until(lk, deadline, [&]{ return !inbox.empty(); });
            if (inbox.empty())
            {
                return false;
            }
            out = std::move(inbox.front());
            inbox.pop_front();
            return true;
        }

        bool SendBinary(std::span<const std::byte> bytes)
        {
            auto ep_sp = ep.lock();
            if (!ep_sp || !connected)
            {
                return false;
            }

            websocketpp::lib::error_code ec;
            ep_sp->send(hdl,
                        reinterpret_cast<const void*>(bytes.data()),
                        bytes.size(),
                        websocketpp::frame::opcode::binary,
                        ec);
            return !ec;
        }
    };

    class RemotePlayer final : public durak::core::Player
    {
    public:
        RemotePlayer(durak::core::PlyrIdxT seat,
                     std::shared_ptr<SeatChannel> chan);

        // Late-bind the authoritative game once constructed
        void BindGame(durak::core::GameImpl& game);

        auto Play(std::shared_ptr<const durak::core::GameSnapshot> snapshot,
                  std::chrono::steady_clock::time_point deadline)
            -> durak::core::PlayerAction override;

        durak::core::PlyrIdxT Seat() const noexcept
        {
            return seat_;
        }

    private:
        durak::core::GameImpl*          game_{nullptr}; // late-bound
        durak::core::PlyrIdxT           seat_{};
        std::shared_ptr<SeatChannel>    chan_;
    };
}

#endif // IDIOTGAME_REMOTEPLAYER_HPP
