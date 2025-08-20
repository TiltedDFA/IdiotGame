#include <gtest/gtest.h>
#include <filesystem>
#include <format>

#include "../core/Game.hpp"
#include "../core/ClassicRules.hpp"
#include "../debug/AuditLogger.hpp"
#include "../core/RandomAi.hpp"

// If you have RandomAI available in your tree, include it here.
// #include "RandomAI.hpp"

using namespace durak::core;

namespace
{

auto make_game(std::uint64_t seed) -> GameImpl
{
    Config cfg{
        .n_players    = 2,
        .deal_up_to   = 6,
        .deck36       = true,
        .seed         = seed,
        .turn_timeout = std::chrono::seconds(2u)
    };

    std::vector<std::unique_ptr<Player>> ps;
    ps.emplace_back(std::make_unique<RandomAI>(seed+1));
    ps.emplace_back(std::make_unique<RandomAI>(seed+2));

    return GameImpl(cfg, std::make_unique<ClassicRules>(), std::move(ps));
}

} // anonymous namespace

TEST(SelfPlay, Transcripts_And_End)
{
    namespace fs = std::filesystem;
    fs::create_directories("_artifacts");

    for (std::uint64_t seed : {111ull, 222ull, 333ull})
    {
        auto game = make_game(seed);
        durak::core::debug::AuditLogger log(std::format("_artifacts/game_{}.log", seed));

        log.start(game, seed);

        for (;;)
        {
            std::uint8_t const actor =
                (game.PhaseNow() == Phase::Defending) ? game.Defender() : game.Attacker();

            auto const snap = game.SnapshotFor(actor);
            log.turn(*snap, actor);   // use the 2-arg version if you don't have the exact action

            MoveOutcome const out = game.Step();
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

        auto const path = fs::path(std::format("_artifacts/game_{}.log", seed));
        ASSERT_TRUE(fs::exists(path));
        ASSERT_GT(fs::file_size(path), 0u);
    }
}
