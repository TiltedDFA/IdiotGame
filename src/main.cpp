//
// Created by Malik T on 13/08/2025.
//

//
// main.cpp — Authoritative match server using WebSocket++
//

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "core/Game.hpp"
#include "core/ClassicRules.hpp"
#include "core/RandomAi.hpp"
#include "core/Judge.hpp"
#include "core/Exception.hpp"
#include "net/RemotePlayer.hpp"
#include "net/codec.hpp"

namespace
{
    using WsServer = websocketpp::server<websocketpp::config::asio>;
    using Hdl      = websocketpp::connection_hdl;

    struct ServerConfig
    {
        std::uint16_t port{9002};
        std::uint32_t n_players{2};
        bool          deck36{true};
        std::uint8_t  deal_up_to{6};
        std::uint64_t seed{123456789ULL};
        std::chrono::milliseconds turn_timeout{std::chrono::seconds(15)};
    };

    auto ParseArgs(int argc, char** argv) -> ServerConfig
    {
        ServerConfig cfg{};

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            auto next_uint = [&](std::uint64_t& out)
            {
                if (i + 1 >= argc) { return false; }
                char const* s = argv[++i];
                auto res = std::from_chars(s, s + std::strlen(s), out);
                return res.ec == std::errc{};
            };

            if (arg == "--port")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.port = static_cast<std::uint16_t>(v); }
            }
            else if (arg == "--players")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.n_players = static_cast<std::uint32_t>(v); }
            }
            else if (arg == "--seed")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.seed = v; }
            }
            else if (arg == "--deal")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.deal_up_to = static_cast<std::uint8_t>(v); }
            }
            else if (arg == "--deck36")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.deck36 = (v != 0); }
            }
            else if (arg == "--timeout_ms")
            {
                std::uint64_t v{};
                if (next_uint(v)) { cfg.turn_timeout = std::chrono::milliseconds(v); }
            }
        }
        return cfg;
    }

    void BroadcastSnapshots(durak::core::GameImpl const& game,
                            std::vector<std::shared_ptr<durak::net::SeatChannel>> const& chans,
                            std::uint64_t msg_id)
    {
        for (durak::core::PlyrIdxT seat = 0; seat < chans.size(); ++seat)
        {
            auto const buf = durak::core::net::BuildSnapshot(game, seat, msg_id);
            std::span<uint8_t const> bytes{ buf.data(), buf.size() };
            std::span<std::byte const> b{
                reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()
            };

            if (chans[seat] && chans[seat]->connected)
            {
                chans[seat]->SendBinary(b);
            }
        }
    }
}

int main(int argc, char** argv)
{
    using namespace durak;
    using namespace durak::core;

    ServerConfig const sc = ParseArgs(argc, argv);

    std::print("[idiotd] starting on port {} with {} player(s)\n",
               sc.port, sc.n_players);

    auto ep = std::make_shared<WsServer>();
    ep->clear_access_channels(websocketpp::log::alevel::all);
    ep->clear_error_channels(websocketpp::log::elevel::all);

    ep->init_asio();
    ep->set_reuse_addr(true);

    std::vector<std::shared_ptr<durak::net::SeatChannel>> chans(sc.n_players);
    for (std::size_t i = 0; i < sc.n_players; ++i)
    {
        chans[i] = std::make_shared<durak::net::SeatChannel>();
        chans[i]->ep = ep;
    }
    std::unordered_map<void*, std::size_t> hdl_to_seat;

    ep->set_open_handler([&](Hdl hdl)
    {
        auto con = ep->get_con_from_hdl(hdl);
        void* key = con.get();

        std::size_t seat = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < chans.size(); ++i)
        {
            if (!chans[i]->connected)
            {
                seat = i;
                break;
            }
        }
        if (seat == static_cast<std::size_t>(-1))
        {
            ep->close(hdl, websocketpp::close::status::try_again_later, "All seats occupied");
            return;
        }

        hdl_to_seat[key] = seat;
        chans[seat]->hdl = hdl;
        chans[seat]->connected = true;

        std::print("[idiotd] client connected -> seat {}\n", seat);

        std::string hello = "SeatAssigned " + std::to_string(seat) +
                            " / " + std::to_string(chans.size());
        websocketpp::lib::error_code ec;
        ep->send(hdl, hello, websocketpp::frame::opcode::text, ec);
    });

    ep->set_close_handler([&](Hdl hdl)
    {
        auto con = ep->get_con_from_hdl(hdl);
        void* key = con.get();

        auto it = hdl_to_seat.find(key);
        if (it != hdl_to_seat.end())
        {
            std::size_t seat = it->second;
            hdl_to_seat.erase(it);

            if (seat < chans.size())
            {
                chans[seat]->connected = false;
                std::print("[idiotd] seat {} disconnected\n", seat);
            }
        }
    });

    ep->set_message_handler([&](Hdl hdl, WsServer::message_ptr msg)
    {
        auto con = ep->get_con_from_hdl(hdl);
        void* key = con.get();

        auto it = hdl_to_seat.find(key);
        if (it == hdl_to_seat.end())
        {
            return;
        }
        std::size_t const seat = it->second;

        if (msg->get_opcode() != websocketpp::frame::opcode::binary)
        {
            return;
        }

        auto const& payload = msg->get_payload();
        std::vector<uint8_t> bytes(payload.begin(), payload.end());

        if (seat < chans.size() && chans[seat])
        {
            chans[seat]->Enqueue(std::move(bytes));
        }
    });

    ep->listen(sc.port);
    ep->start_accept();
    std::thread net_thr([ep]
    {
        ep->run();
    });

    Config cfg;
    cfg.n_players    = sc.n_players;
    cfg.deal_up_to   = sc.deal_up_to;
    cfg.deck36       = sc.deck36;
    cfg.seed         = sc.seed;
    cfg.turn_timeout = sc.turn_timeout;

    auto rules = std::make_unique<ClassicRules>();

    // Build players up front. For connected seats we use RemotePlayer (late-bound).
    std::vector<std::unique_ptr<Player>> players;
    players.reserve(sc.n_players);

    std::vector<durak::net::RemotePlayer*> remote_ptrs; // to bind after GameImpl exists

    for (std::size_t i = 0; i < sc.n_players; ++i)
    {
        if (chans[i] && chans[i]->connected)
        {
            auto rp = std::make_unique<durak::net::RemotePlayer>(static_cast<PlyrIdxT>(i), chans[i]);
            remote_ptrs.push_back(rp.get());
            players.emplace_back(std::move(rp));
        }
        else
        {
            players.emplace_back(std::make_unique<RandomAI>(sc.seed + static_cast<uint64_t>(i * 1337u)));
        }
    }

    GameImpl game(cfg, std::move(rules), std::move(players));

    // Late-bind authoritative game to all remote players
    for (auto* rp : remote_ptrs)
    {
        rp->BindGame(game);
    }

    std::uint64_t msg_counter{1};
    BroadcastSnapshots(game, chans, msg_counter++);

    MoveOutcome outcome = MoveOutcome::Applied;

    while (outcome != MoveOutcome::GameEnded)
    {
        outcome = game.Step();

        // TODO (post-MVP): emit Violation envelopes when Step determines Invalid w/ reason.
        BroadcastSnapshots(game, chans, msg_counter++);
    }

    std::print("[idiotd] game over\n");

    ep->stop_listening();
    ep->stop();
    if (net_thr.joinable())
    {
        net_thr.join();
    }

    return 0;
}
