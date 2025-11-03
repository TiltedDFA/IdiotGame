
#ifndef IDIOTGAME_CODEC_HPP
#define IDIOTGAME_CODEC_HPP

#include <cstddef>   // std::byte
#include <cstdint>
#include <span>
#include <variant>
#include <vector>
#include <string>
#include <expected>
#include <flatbuffers/flatbuffers.h>

// Core (PascalCase, existing)
#include "../core/Types.hpp"
#include "../core/Actions.hpp"
#include "../core/State.hpp"
#include "../core/Game.hpp"
#include "../core/Exception.hpp"

#include "../generated/flatbuffers/durak_net_generated.h"

namespace durak::core::net
{
    // Lightweight local parse error (as permitted)
    struct ParseError
    {
        std::string message;
    };

    // What a player action decodes into
    struct DecodedAction
    {
        durak::core::PlyrIdxT actor{};
        durak::core::PlayerAction action{};
    };

    // Value-side card used by clients over the wire
    struct CardVal
    {
        Suit suit{};
        Rank rank{};
    };

    struct DefPair
    {
        CardVal attack{};
        CardVal defend{};
    };

    auto ToFbSuit(durak::core::Suit s) noexcept -> durak::gen::net::Suit;
    auto ToFbRank(durak::core::Rank r) noexcept -> durak::gen::net::Rank;
    auto ToFbPhase(durak::core::Phase p) noexcept -> durak::gen::net::Phase;

    auto FromFbSuit(durak::gen::net::Suit s) noexcept -> durak::core::Suit;
    auto FromFbRank(durak::gen::net::Rank r) noexcept -> durak::core::Rank;
    auto FromFbPhase(durak::gen::net::Phase p) noexcept -> durak::core::Phase;

    // ----- Value-based builders (FOR CLIENTS) -----
    // These build an Envelope::PlayerAction directly from suit/rank values.
    auto BuildAction_Attack(PlyrIdxT actor,
                            std::span<const CardVal> cards,
                            std::uint64_t msg_id) -> std::vector<std::uint8_t>;

    auto BuildAction_Defend(PlyrIdxT actor,
                            std::span<const DefPair> pairs,
                            std::uint64_t msg_id) -> std::vector<std::uint8_t>;
    // --- Outbound builders (server → client, client → server) ---

    auto BuildSnapshot(durak::core::GameImpl const& g,
                       durak::core::PlyrIdxT seat,
                       std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    auto BuildViolation(durak::core::error::RuleViolation const& v,
                        std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    // Attack: provide weak refs from the actor’s hand
    auto BuildAction_Attack(durak::core::PlyrIdxT actor,
                            std::span<durak::core::CardWP const> cards,
                            std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    // Defend: provide pairs of (attack-from-table, defend-from-hand)
    auto BuildAction_Defend(durak::core::PlyrIdxT actor,
                            std::span<durak::core::DefendPair const> pairs,
                            std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    auto BuildAction_Pass(durak::core::PlyrIdxT actor,
                          std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    auto BuildAction_Take(durak::core::PlyrIdxT actor,
                          std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer;

    // --- Inbound decode (envelope → (actor, PlayerAction)) ---

    auto DecodePlayerAction(durak::core::GameImpl& g,
                            std::span<std::byte const> bytes)
        -> std::expected<DecodedAction, ParseError>;
} // namespace durak::core::net


#endif //IDIOTGAME_CODEC_HPP
