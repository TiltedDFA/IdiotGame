//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_EXCEPTION_HPP
#define IDIOTGAME_EXCEPTION_HPP

#include "OmegaException.hpp"

#include <expected>
#include <stdexcept>
#include <format>
#include <utility>
#include "Types.hpp"
#include "Actions.hpp"
// #if DRK_ALLOW_EXCEPTIONS == true
// #define DRK_OOPS(msg,data) throw durak::core::OmegaException<decltype(data)>((msg),(data))
// #define DRK_COND_OOPS(cond,msg,data) if((cond)) throw durak::core::OmegaException<decltype(data)>((msg),(data))
// #else
// #define DRK_OOPS(msg,data)
// #define DRK_COND_OOPS(cond,msg,data)
// #endif

namespace durak::core::error
{
    enum class Code : unsigned
    {
        Unknown, // unknown error
        Rules, // rules engine misuse (not user invalid move)
        State, // state engine misuse (not user invalid move)
        InvalidAction, // user/remote proposed action cannot be applied
        Timeout, // deadline exceeded for IO or player move
        Network, // transport failure
        Serialization, // FlatBuffers verification/build errors
        Assertion // internal assertion failed
    };

    struct UnknownError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct RulesError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct StateError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct InvalidActionError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct TimeoutError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct NetworkError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct SerializationError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    struct AssertionError : public OmegaException<Code>
    {
        using OmegaException<Code>::OmegaException;
    };

    [[noreturn]]
    inline auto fail(Code c, std::string msg) -> void
    {
        switch (c)
        {
        case Code::Unknown: throw UnknownError(std::move(msg), c);
        case Code::Rules: throw RulesError(std::move(msg), c);
        case Code::State: throw StateError(std::move(msg), c);
        case Code::InvalidAction: throw InvalidActionError(std::move(msg), c);
        case Code::Timeout: throw TimeoutError(std::move(msg), c);
        case Code::Network: throw NetworkError(std::move(msg), c);
        case Code::Serialization: throw SerializationError(std::move(msg), c);
        case Code::Assertion: throw AssertionError(std::move(msg), c);
        }
        throw std::runtime_error(msg);
    }

#define DRK_THROW(code_enum, msg) ::durak::core::error::fail((code_enum), (msg))
#define DRK_ASSERT(cond, msg) do { if(!(cond)) ::durak::core::error::fail(::durak::core::error::Code::Assertion, (msg)); } while(0)

    // Fine-grained reasons; grouped by action type.
    enum class RuleViolationCode : std::uint16_t
    {
        // Generic/flow
        WrongPhase_AttackingRequired,
        WrongPhase_DefendingRequired,
        WrongActor_AttackerRequired,
        WrongActor_DefenderRequired,

        // Attack
        Attack_Empty,
        Attack_TooManyForCapacity,
        Attack_PointersInvalid,
        Attack_CardNotOwnedByAttacker,
        Attack_RankNotOnTableWhenRequired,
        Attack_DuplicateCards,

        // Defend
        Defend_Empty,
        Defend_PointersInvalid,
        Defend_AttackNotOnTable,
        Defend_AttackAlreadyCovered,
        Defend_CardNotOwnedByDefender,
        Defend_DoesNotBeat,
        Defend_DuplicateCards,
        Defend_UncoveredPairsMismatch, // uncovered != pairs.size()

        // Pass
        Pass_WrongPhase,
        Pass_NotAttacker,
        Pass_TableEmpty,
        Pass_UncoveredRemain,

        // Take
        Take_WrongPhase,
        Take_NotDefender,

        // Safety net
        Internal_Unreachable
    };

    // Compact, optional context carried with the violation.
    struct RuleViolation
    {
        RuleViolationCode code{};
        std::optional<Phase> phase{};
        std::optional<PlyrIdxT> actor{};
        std::optional<PlyrIdxT> attacker{};
        std::optional<PlyrIdxT> defender{};

        // Small integers useful in error messages
        std::optional<std::uint8_t> capacity_used{};
        std::optional<std::uint8_t> capacity_free{};
        std::optional<std::uint8_t> defender_hand{};
        std::optional<std::uint8_t> attempted_count{}; // e.g., number of cards/pairs

        // Card-related details
        std::optional<Rank> rank{}; // e.g., rank required on table

        // Quick helpers to build enriched violations (fluent style).
        auto with_phase(Phase p) -> RuleViolation&
        {
            phase = p;
            return *this;
        }

        auto with_actor(PlyrIdxT s) -> RuleViolation&
        {
            actor = s;
            return *this;
        }

        auto with_attacker(PlyrIdxT s) -> RuleViolation&
        {
            attacker = s;
            return *this;
        }

        auto with_defender(PlyrIdxT s) -> RuleViolation&
        {
            defender = s;
            return *this;
        }

        auto with_cap_used(std::uint8_t v) -> RuleViolation&
        {
            capacity_used = v;
            return *this;
        }

        auto with_cap_free(std::uint8_t v) -> RuleViolation&
        {
            capacity_free = v;
            return *this;
        }

        auto with_def_hand(std::uint8_t v) -> RuleViolation&
        {
            defender_hand = v;
            return *this;
        }

        auto with_attempted(std::uint8_t v) -> RuleViolation&
        {
            attempted_count = v;
            return *this;
        }

        auto with_rank(Rank r) -> RuleViolation&
        {
            rank = r;
            return *this;
        }
    };

    inline auto to_string(RuleViolationCode c) -> std::string_view
    {
        using E = RuleViolationCode;
        switch (c)
        {
        // Flow
        case E::WrongPhase_AttackingRequired: return "Wrong phase (attacking required)";
        case E::WrongPhase_DefendingRequired: return "Wrong phase (defending required)";
        case E::WrongActor_AttackerRequired: return "Wrong actor (attacker required)";
        case E::WrongActor_DefenderRequired: return "Wrong actor (defender required)";

        // Attack
        case E::Attack_Empty: return "Attack: empty card list";
        case E::Attack_TooManyForCapacity: return "Attack: exceeds capacity";
        case E::Attack_PointersInvalid: return "Attack: invalid/expired card reference";
        case E::Attack_CardNotOwnedByAttacker: return "Attack: card not owned by attacker";
        case E::Attack_RankNotOnTableWhenRequired: return "Attack: rank not present on table";
        case E::Attack_DuplicateCards: return "Attack: duplicate cards in action";

        // Defend
        case E::Defend_Empty: return "Defend: empty pair list";
        case E::Defend_PointersInvalid: return "Defend: invalid/expired reference";
        case E::Defend_AttackNotOnTable: return "Defend: referenced attack not on table";
        case E::Defend_AttackAlreadyCovered: return "Defend: attack already covered";
        case E::Defend_CardNotOwnedByDefender: return "Defend: card not owned by defender";
        case E::Defend_DoesNotBeat: return "Defend: defending card does not beat attack";
        case E::Defend_DuplicateCards: return "Defend: duplicate cards in action";
        case E::Defend_UncoveredPairsMismatch: return "Defend: uncovered count != pairs.size()";

        // Pass/Take
        case E::Pass_WrongPhase: return "Pass: wrong phase";
        case E::Pass_NotAttacker: return "Pass: only attacker may pass";
        case E::Pass_TableEmpty: return "Pass: table is empty";
        case E::Pass_UncoveredRemain: return "Pass: uncovered attacks remain";
        case E::Take_WrongPhase: return "Take: wrong phase";
        case E::Take_NotDefender: return "Take: only defender may take";

        case E::Internal_Unreachable: return "Internal: unreachable";
        }
        return "Unknown";
    }

    inline auto describe(RuleViolation const& v) -> std::string
    {
        // Build a compact, reproducible message for logs/tests.
        auto s = std::format("{}", to_string(v.code));
        if (v.phase) s += std::format(" | phase={}",
                                      (*v.phase == Phase::Attacking
                                           ? "A"
                                           : (*v.phase == Phase::Defending ? "D" : "C")));
        if (v.actor) s += std::format(" | actor=P{}", static_cast<int>(*v.actor));
        if (v.attacker) s += std::format(" | atk=P{}", static_cast<int>(*v.attacker));
        if (v.defender) s += std::format(" | def=P{}", static_cast<int>(*v.defender));
        if (v.capacity_used) s += std::format(" | used={}", *v.capacity_used);
        if (v.capacity_free) s += std::format(" | free={}", *v.capacity_free);
        if (v.defender_hand) s += std::format(" | defHand={}", *v.defender_hand);
        if (v.attempted_count)s += std::format(" | attempted={}", *v.attempted_count);
        if (v.rank) s += std::format(" | rank={}", static_cast<int>(std::to_underlying(*v.rank)));
        return s;
    }

    // New alias (unchanged name used by the codebase)
    using ValidateResult = std::expected<void, RuleViolation>;
}

#endif //IDIOTGAME_EXCEPTION_HPP