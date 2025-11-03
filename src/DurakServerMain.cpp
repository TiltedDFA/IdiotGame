// File: src/apps/DurakServerMain.cpp
//
// Allman style. Explicit types. No K&R.
//
// A minimal authoritative server using WebSocket++ (no TLS) + standalone Asio.
// Waits for N seats to connect, then runs the game to completion.
// Each seat is driven by a blocking remote Player that receives snapshots and
// must respond with a PlayerAction message (or times out -> Pass/Take fallback).

#include <cstdint>
#include <deque>
#include <mutex>
#include <map>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <cstdio>
#include <print>
#include <vector>
#include <string>
#include <optional>
#include <chrono>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "core/Game.hpp"
#include "core/ClassicRules.hpp"
#include "core/RandomAi.hpp"   // not used by server-side seats, here for config parity/logs
#include "core/Exception.hpp"
#include "net/codec.hpp"       // BuildSnapshot, BuildAction_*, DecodePlayerAction

// Generated FB headers are available via include path set in CMake.
#include "generated/flatbuffers/durak_net_generated.h"

namespace
{
    using WsServer = websocketpp::server<websocketpp::config::asio>;

    struct Frame
    {
        std::vector<std::uint8_t> bytes;
    };

    class InboundQueue
    {
    public:
        InboundQueue() = default;

        void push(Frame f)
        {
            std::lock_guard<std::mutex> lock(m_);
            q_.push_back(std::move(f));
            cv_.notify_one();
        }

        // Pop a frame until absolute deadline; returns false on timeout.
        bool pop_until(std::chrono::steady_clock::time_point deadline, Frame& out)
        {
            std::unique_lock<std::mutex> lock(m_);
            while (q_.empty())
            {
                if (cv_.wait_until(lock, deadline) == std::cv_status::timeout)
                {
                    return false;
                }
            }
            out = std::move(q_.front());
            q_.pop_front();
            return true;
        }

        // Non-blocking pop (for drains / close)
        bool try_pop(Frame& out)
        {
            std::lock_guard<std::mutex> lock(m_);
            if (q_.empty())
            {
                return false;
            }
            out = std::move(q_.front());
            q_.pop_front();
            return true;
        }

    private:
        std::mutex m_;
        std::condition_variable cv_;
        std::deque<Frame> q_;
    };

    // A server-side Player adapter bound to a seat and a socket send function.
    // It blocks inside Play() waiting for a PlayerAction frame from the network.
    class WsRemotePlayer final : public durak::core::Player
    {
    public:
        using SendFn = std::function<void(const std::uint8_t *, std::size_t)>;

        WsRemotePlayer(durak::core::PlyrIdxT seat,
                       std::shared_ptr<InboundQueue> inbox,
                       SendFn send)
            : seat_(seat)
              , inbox_(std::move(inbox))
              , send_(std::move(send))
              , next_msg_id_(1ULL)
        {
        }

        durak::core::PlayerAction Play(std::shared_ptr<const durak::core::GameSnapshot> snapshot,
                                       std::chrono::steady_clock::time_point deadline) override
        {
            // 1) Push a fresh snapshot to this seat (so their UI/AI is up to date)
            flatbuffers::DetachedBuffer buf =
                durak::core::net::BuildSnapshot(*snapshot_owner_, seat_, next_msg_id_++);

            const std::uint8_t* data_ptr = buf.data();
            const std::size_t data_len = buf.size();

            send_(data_ptr, data_len);

            // 2) Wait for a PlayerActionMsg until deadline; on timeout -> Pass/Take fallback.
            Frame f{};
            bool got = inbox_->pop_until(deadline, f);
            if (!got)
            {
                // Use snapshot phase to choose fallback.
                if (snapshot->phase == durak::core::Phase::Defending)
                {
                    std::print("[Seat {}] Play timeout -> Take\n", static_cast<int>(seat_));
                    return durak::core::TakeAction{};
                }
                else
                {
                    std::print("[Seat {}] Play timeout -> Pass\n", static_cast<int>(seat_));
                    return durak::core::PassAction{};
                }
            }

            // 3) Decode and validate envelope locally (parsing only; rules are on Game side)
            std::span<const std::byte> bytes{
                reinterpret_cast<const std::byte*>(f.bytes.data()),
                f.bytes.size()
            };

            std::expected<durak::core::net::DecodedAction, durak::core::net::ParseError> parsed =
                durak::core::net::DecodePlayerAction(*game_, bytes);

            if (!parsed.has_value())
            {
                std::print("[Seat {}] Parse error: {}\n", static_cast<int>(seat_), parsed.error().message);
                // Keep waiting until deadline exhausted? Simple approach: fall back now.
                if (snapshot->phase == durak::core::Phase::Defending)
                {
                    return durak::core::TakeAction{};
                }
                return durak::core::PassAction{};
            }

            // 4) Anti-spoof: actor in message must match seat bound to this connection.
            if (parsed->actor != seat_)
            {
                std::print("[Seat {}] Spoofed actor {} -> rejected\n",
                           static_cast<int>(seat_), static_cast<int>(parsed->actor));
                if (snapshot->phase == durak::core::Phase::Defending)
                {
                    return durak::core::TakeAction{};
                }
                return durak::core::PassAction{};
            }

            return parsed->action;
        }

        // Bound by server right after creating GameImpl (so BuildSnapshot can use it)
        void BindGameAndSnapshotOwner(durak::core::GameImpl* game, const durak::core::GameImpl* snapshot_owner)
        {
            game_ = game;
            snapshot_owner_ = snapshot_owner;
        }

    private:
        durak::core::PlyrIdxT seat_;
        std::shared_ptr<InboundQueue> inbox_;
        SendFn send_;
        std::uint64_t next_msg_id_;

        // Pointers valid for the lifetime of the match
        durak::core::GameImpl* game_{nullptr};
        const durak::core::GameImpl* snapshot_owner_{nullptr};
    };

    struct SeatConn
    {
        durak::core::PlyrIdxT seat{0};
        websocketpp::connection_hdl hdl{};
        std::shared_ptr<InboundQueue> inbox;
        std::shared_ptr<WsRemotePlayer> player;
        bool connected{false};
    };

    struct CmdLine
    {
        std::uint16_t port{9002};
        std::uint8_t players{2};
        std::uint64_t seed{12345ULL};
        bool deck36{true};
        std::uint8_t deal_up_to{6};
        std::uint32_t turn_timeout_ms{15000};
    };

    CmdLine parse_args(int argc, char** argv)
    {
        CmdLine c{};
        for (int i = 1; i < argc; ++i)
        {
            std::string key = argv[i];
            auto read_u64 = [&](std::uint64_t& dst)
            {
                if (i + 1 < argc)
                {
                    dst = std::strtoull(argv[++i], nullptr, 10);
                }
            };
            auto read_u32 = [&](std::uint32_t& dst)
            {
                if (i + 1 < argc)
                {
                    dst = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
                }
            };
            auto read_u16 = [&](std::uint16_t& dst)
            {
                if (i + 1 < argc)
                {
                    dst = static_cast<std::uint16_t>(std::strtoul(argv[++i], nullptr, 10));
                }
            };
            auto read_u8 = [&](std::uint8_t& dst)
            {
                if (i + 1 < argc)
                {
                    dst = static_cast<std::uint8_t>(std::strtoul(argv[++i], nullptr, 10));
                }
            };

            if (key == "--port") { read_u16(c.port); }
            else if (key == "--players") { read_u8(c.players); }
            else if (key == "--seed") { read_u64(c.seed); }
            else if (key == "--deal-up-to") { read_u8(c.deal_up_to); }
            else if (key == "--turn-timeout-ms") { read_u32(c.turn_timeout_ms); }
            else if (key == "--deck36")
            {
                c.deck36 = true;
            }
            else if (key == "--deck52")
            {
                c.deck36 = false;
            }
        }
        if (c.players < 2)
        {
            c.players = 2;
        }
        return c;
    }
} // anon

int main(int argc, char** argv)
{
    CmdLine cfg = parse_args(argc, argv);

    std::print("[Server] Booting on port {} | waiting for {} player(s)\n",
               cfg.port, static_cast<int>(cfg.players));

    WsServer server;
    server.clear_access_channels(websocketpp::log::alevel::all);
    server.set_access_channels(websocketpp::log::alevel::connect |
        websocketpp::log::alevel::disconnect);
    server.init_asio();

    std::mutex seats_mx;
    std::condition_variable seats_cv;
    std::vector<std::shared_ptr<SeatConn>> seats;
    seats.reserve(cfg.players);
    for (std::uint8_t i = 0; i < cfg.players; ++i)
    {
        std::shared_ptr<SeatConn> sc = std::make_shared<SeatConn>();
        sc->seat = i;
        sc->inbox = std::make_shared<InboundQueue>();
        seats.push_back(std::move(sc));
    }

    std::atomic<std::uint8_t> connected_count{0};

    using Hdl = websocketpp::connection_hdl;
    // Map hdl -> seat index
    std::mutex map_mx;
    std::map<Hdl, uint8_t, std::owner_less<Hdl>> hdl_to_seat;

    server.set_open_handler([&](websocketpp::connection_hdl hdl)
    {
        std::lock_guard<std::mutex> lock(seats_mx);
        if (connected_count.load() >= cfg.players)
        {
            std::print("[Server] Extra connection rejected (seats full)\n");
            try
            {
                server.close(hdl, websocketpp::close::status::policy_violation, "Seats full");
            }
            catch (...)
            {
            }
            return;
        }

        std::uint8_t seat = connected_count.load();
        seats[seat]->hdl = hdl;
        seats[seat]->connected = true;
        ++connected_count;

        {
            std::lock_guard<std::mutex> g(map_mx);
            hdl_to_seat[hdl] = seat;
        }

        std::print("[Server] Seat {} connected ({} of {})\n",
                   static_cast<int>(seat),
                   static_cast<int>(connected_count.load()),
                   static_cast<int>(cfg.players));

        seats_cv.notify_all();
    });

    server.set_close_handler([&](websocketpp::connection_hdl hdl)
    {
        std::optional<std::uint8_t> seat_opt;
        {
            std::lock_guard<std::mutex> g(map_mx);
            auto it = hdl_to_seat.find(hdl);
            if (it != hdl_to_seat.end())
            {
                seat_opt = it->second;
                hdl_to_seat.erase(it);
            }
        }
        if (seat_opt.has_value())
        {
            std::lock_guard<std::mutex> lock(seats_mx);
            std::uint8_t seat = *seat_opt;
            if (seats[seat]->connected)
            {
                seats[seat]->connected = false;
                if (connected_count.load() > 0)
                {
                    connected_count--;
                }
                std::print("[Server] Seat {} disconnected\n", static_cast<int>(seat));
                seats_cv.notify_all();
            }
        }
    });

    server.set_message_handler([&](websocketpp::connection_hdl hdl, WsServer::message_ptr msg)
    {
        // Only binary frames are valid
        if (msg->get_opcode() != websocketpp::frame::opcode::binary)
        {
            std::print("[Server] Ignoring non-binary frame from client\n");
            return;
        }

        std::uint8_t seat = 0xFF;
        {
            std::lock_guard<std::mutex> g(map_mx);
            auto it = hdl_to_seat.find(hdl);
            if (it == hdl_to_seat.end())
            {
                return;
            }
            seat = it->second;
        }

        std::string const& payload = msg->get_payload();
        Frame f{};
        f.bytes.assign(reinterpret_cast<const std::uint8_t*>(payload.data()),
                       reinterpret_cast<const std::uint8_t*>(payload.data()) + payload.size());

        seats[seat]->inbox->push(std::move(f));
    });

    // Start network
    server.listen(cfg.port);
    server.start_accept();
    std::thread net_thr([&server]()
    {
        server.run();
    });

    // Wait until all seats are connected
    {
        std::unique_lock<std::mutex> lk(seats_mx);
        seats_cv.wait(lk, [&]()
        {
            return connected_count.load() >= cfg.players;
        });
    }

    std::print("[Server] All players connected. Starting match…\n");

    // Build players (all network seats)
    std::vector<std::unique_ptr<durak::core::Player>> players;
    players.reserve(cfg.players);

    // Helper to send binary to a seat
    auto make_send_fn = [&](std::uint8_t seat) -> WsRemotePlayer::SendFn
    {
        return [seat, &server, &seats](const std::uint8_t* data, std::size_t len)
        {
            websocketpp::connection_hdl hdl = seats[seat]->hdl;
            try
            {
                server.send(hdl, data, len, websocketpp::frame::opcode::binary);
            }
            catch (const std::exception& e)
            {
                std::print("[Server] send() error seat {}: {}\n", static_cast<int>(seat), e.what());
            }
            catch (...)
            {
            }
        };
    };

    for (std::uint8_t s = 0; s < cfg.players; ++s)
    {
        std::shared_ptr<WsRemotePlayer> rp = std::make_shared<WsRemotePlayer>(
            s,
            seats[s]->inbox,
            make_send_fn(s)
        );
        seats[s]->player = rp;
        players.push_back(std::unique_ptr<durak::core::Player>(rp.get()));
        // NOTE: we will keep shared_ptr alive via seats[s]->player.
        // The unique_ptr in players does NOT own, but points to the same object.
        // To keep ownership clear, allocate raw and wrap both? For simplicity,
        // allocate once and release unique_ptr before leaving.
    }

    // Build GameImpl
    durak::core::Config gcfg{};
    gcfg.n_players = cfg.players;
    gcfg.deal_up_to = cfg.deal_up_to;
    gcfg.deck36 = cfg.deck36;
    gcfg.seed = cfg.seed;
    gcfg.turn_timeout = std::chrono::milliseconds(cfg.turn_timeout_ms);

    std::unique_ptr<durak::core::Rules> rules = std::make_unique<durak::core::ClassicRules>();
    durak::core::GameImpl game(gcfg, std::move(rules), std::move(players));

    // Bind snapshot/game pointers for snapshot-sending in WsRemotePlayer
    for (std::uint8_t s = 0; s < cfg.players; ++s)
    {
        if (seats[s]->player)
        {
            seats[s]->player->BindGameAndSnapshotOwner(&game, &game);
        }
    }

    // Helper: broadcast a snapshot to every seat
    auto broadcast_snapshot = [&](std::uint64_t msg_id_base)
    {
        for (std::uint8_t s = 0; s < cfg.players; ++s)
        {
            std::shared_ptr<const durak::core::GameSnapshot> snap = game.SnapshotFor(s);
            flatbuffers::DetachedBuffer buf =
                durak::core::net::BuildSnapshot(game, s, msg_id_base + s);

            try
            {
                server.send(seats[s]->hdl, buf.data(), buf.size(), websocketpp::frame::opcode::binary);
            }
            catch (...)
            {
            }
        }
    };

    // Initial broadcast so clients can render something immediately
    broadcast_snapshot(/*msg_id_base*/1000);

    // Main loop
    std::uint64_t step_no = 0;
    while (true)
    {
        durak::core::MoveOutcome out = game.Step();
        step_no++;

        std::print("[Server] Step {} -> outcome {}\n",
                   step_no, static_cast<int>(out));

        broadcast_snapshot(/*msg_id_base*/2000 + step_no * 10);

        if (out == durak::core::MoveOutcome::GameEnded)
        {
            std::print("[Server] Match ended after {} steps.\n", step_no);
            break;
        }
    }

    // Keep server up a moment to flush frames
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    try
    {
        server.stop_listening();
        // Close all connections politely
        for (std::uint8_t s = 0; s < cfg.players; ++s)
        {
            try
            {
                server.close(seats[s]->hdl, websocketpp::close::status::going_away, "Game over");
            }
            catch (...)
            {
            }
        }
    }
    catch (...)
    {
    }

    if (net_thr.joinable())
    {
        net_thr.join();
    }

    return 0;
}