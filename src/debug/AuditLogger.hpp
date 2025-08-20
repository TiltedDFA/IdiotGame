//
// Created by Malik T on 20/08/2025.
//

#ifndef IDIOTGAME_AUDITLOGGER_HPP
#define IDIOTGAME_AUDITLOGGER_HPP

#include <cstdint>
#include <fstream>
#include <string>

#include "../core/Game.hpp"
#include "../core/State.hpp"
#include "../core/Actions.hpp"
#include "../core/Types.hpp"

namespace durak::core::debug
{
    class AuditLogger
    {
    public:
        explicit AuditLogger(std::string path);
        ~AuditLogger();

        AuditLogger(AuditLogger const&) = delete;
        auto operator=(AuditLogger const&) -> AuditLogger& = delete;
        
        AuditLogger(AuditLogger&&) noexcept = default;
        auto operator=(AuditLogger&&) noexcept -> AuditLogger& = default;

        // Session header (seed, trump, player count)
        auto start(GameImpl const& game, std::uint64_t seed) -> void;

        // Per turn (before Apply/Advance): snapshot, actor seat, proposed action (preferred)
        auto turn(GameSnapshot const& s,
                  std::uint8_t actor,
                  PlayerAction const& a) -> void;

        // Per turn (fallback when action is unavailable in black-box tests)
        auto turn(GameSnapshot const& s,
                  std::uint8_t actor) -> void;

        // Per step outcome (after Apply/Advance)
        auto outcome(MoveOutcome m) -> void;

        // After a cleanup round, log all hand sizes by seat (via snapshots)
        auto cleanup(GameImpl const& game) -> void;

        // Game end footer (loser seat; -1 if none)
        auto end(GameImpl const& game) -> void;

        // Manual flush
        auto flush() -> void;

    private:
        std::ofstream out_;
    };
}

#endif //IDIOTGAME_AUDITLOGGER_HPP