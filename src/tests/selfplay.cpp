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

// If you have RandomAI available in your tree, include it here.
// #include "RandomAI.hpp"

using namespace durak::core;

namespace
{
auto make_players(uint64_t seed) -> std::vector<std::unique_ptr<Player>>
{
    std::vector<std::unique_ptr<Player>> ps;
    ps.emplace_back(std::make_unique<RandomAI>(seed + 1));
    ps.emplace_back(std::make_unique<RandomAI>(seed + 2));
    return ps;
}
auto make_game(std::uint64_t seed) -> GameImpl
{
    Config cfg{
        .n_players    = 2,
        .deal_up_to   = 6,
        .deck36       = true,
        .seed         = seed,
        .turn_timeout = std::chrono::seconds(2u)
    };

    auto ps = make_players(seed);
    ps = durak::core::debug::WrapRecording(ps);

    return GameImpl(cfg, std::make_unique<ClassicRules>(), std::move(ps));
}

} // anonymous namespace

TEST(SelfPlay, Transcripts_And_End)
{
    namespace fs = std::filesystem;
    fs::create_directories("_artifacts");
    try
    {
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
                // log.turn(*snap, actor);   // use the 2-arg version if you don't have the exact action


                // We will log the actual action after Step(), using RecordingPlayer.
                MoveOutcome const out = game.Step();
                // durak::core::debug::CheckInvariants(game);

                // Fetch the action chosen by the actor
                auto* rec = durak::core::debug::AsRecording(game.PlayerAt(actor));
                ASSERT_NE(rec, nullptr) << "Player not wrapped with RecordingPlayer";
                ASSERT_TRUE(rec->HasLast()) << "No action recorded for actor seat";

                log.turn(*snap, actor, rec->Last());
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
    catch (durak::core::OmegaException<durak::core::error::Code> const& e)
    {
        std::print("{}", e.to_str());
    }
}
