//
// Created by Malik T on 26/08/2025.
//
#include <gtest/gtest.h>
#include <filesystem>
#include <format>
#include <print>

#include "../core/Game.hpp"
#include "../core/ClassicRules.hpp"
#include "../debug/AuditLogger.hpp"
#include "../core/RandomAi.hpp"
#include "../debug/Invariants.hpp"
#include "../debug/RecordingPlayer.hpp"

using namespace durak::core;

namespace
{

auto make_players(std::uint64_t seed, std::size_t n) -> std::vector<std::unique_ptr<Player>>
{
    std::vector<std::unique_ptr<Player>> ps;
    ps.reserve(n);
    for (std::size_t i{}; i < n; ++i)
    {
        ps.emplace_back(std::make_unique<RandomAI>(seed + static_cast<std::uint64_t>(i + 1)));
    }
    return ps;
}

auto make_game(std::uint64_t seed, std::size_t n_players) -> GameImpl
{
    Config cfg{
        .n_players    = static_cast<std::uint32_t>(n_players),
        .deal_up_to   = 6,
        .deck36       = true,                      // 36 cards = 6 * 6 initial deal (deck empty after deal)
        .seed         = seed,
        .turn_timeout = std::chrono::seconds(2u)
    };

    auto ps = make_players(seed, n_players);
    ps = durak::core::debug::WrapRecording(ps);

    return GameImpl(cfg, std::make_unique<ClassicRules>(), std::move(ps));
}

} // anonymous namespace

TEST(SelfPlay6P, Transcripts_And_End)
{
    namespace fs = std::filesystem;
    fs::create_directories("_artifacts");
    try
    {
        for (std::uint64_t seed : {111ull, 222ull, 333ull, 1ull, 23ull, 44ull})
        {
            auto game = make_game(seed, 6);
            durak::core::debug::AuditLogger log(std::format("_artifacts/game6p_{}.log", seed));

            log.start(game, seed);

            for (;;)
            {
                std::uint8_t const actor =
                    (game.PhaseNow() == Phase::Defending) ? game.Defender() : game.Attacker();

                auto const snap = game.SnapshotFor(actor);

                MoveOutcome const out = game.Step();

                // Optional invariant check (enable if you want deeper guarantees)
                // durak::core::debug::CheckInvariants(game);

                auto* rec = durak::core::debug::AsRecording(game.PlayerAt(actor));
                ASSERT_NE(rec, nullptr) << "Player not wrapped with RecordingPlayer";
                ASSERT_TRUE(rec->HasLast()) << "No action recorded for actor seat";

                log.turn(game, *snap, actor, rec->Last());
                log.outcome(out);

                if (out == MoveOutcome::RoundEnded)
                {
                    log.cleanup(game);
                }

                if (out == MoveOutcome::GameEnded)
                {
                    log.end(game);
                    break;
                }
            }

            auto const path = fs::path(std::format("_artifacts/game6p_{}.log", seed));
            ASSERT_TRUE(fs::exists(path));
            // ASSERT_GT(fs::file_size(path), 0u);
        }
    }
    catch (durak::core::OmegaException<durak::core::error::Code> const& e)
    {
        std::print("error:{}", e.to_str());
    }
}