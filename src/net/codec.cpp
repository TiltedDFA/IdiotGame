//
// Codec.cpp
//
#include "Codec.hpp"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

// FlatBuffers schema (adjust path if your build outputs elsewhere)
#include "../generated/flatbuffers/durak_net_generated.h"

namespace durak::core::net
{
    auto ToFbSuit(durak::core::Suit s) noexcept -> durak::gen::net::Suit
    {
        switch (s)
        {
        case durak::core::Suit::Clubs: return durak::gen::net::Suit::Clubs;
        case durak::core::Suit::Diamonds: return durak::gen::net::Suit::Diamonds;
        case durak::core::Suit::Hearts: return durak::gen::net::Suit::Hearts;
        case durak::core::Suit::Spades: return durak::gen::net::Suit::Spades;
        }
        return durak::gen::net::Suit::Clubs;
    }

    auto FromFbSuit(durak::gen::net::Suit s) noexcept -> durak::core::Suit
    {
        switch (s)
        {
        case durak::gen::net::Suit::Clubs: return durak::core::Suit::Clubs;
        case durak::gen::net::Suit::Diamonds: return durak::core::Suit::Diamonds;
        case durak::gen::net::Suit::Hearts: return durak::core::Suit::Hearts;
        case durak::gen::net::Suit::Spades: return durak::core::Suit::Spades;
        }
        return durak::core::Suit::Clubs;
    }

    auto ToFbRank(durak::core::Rank r) noexcept -> durak::gen::net::Rank
    {
        switch (r)
        {
        case durak::core::Rank::Two: return durak::gen::net::Rank::Two;
        case durak::core::Rank::Three: return durak::gen::net::Rank::Three;
        case durak::core::Rank::Four: return durak::gen::net::Rank::Four;
        case durak::core::Rank::Five: return durak::gen::net::Rank::Five;
        case durak::core::Rank::Six: return durak::gen::net::Rank::Six;
        case durak::core::Rank::Seven: return durak::gen::net::Rank::Seven;
        case durak::core::Rank::Eight: return durak::gen::net::Rank::Eight;
        case durak::core::Rank::Nine: return durak::gen::net::Rank::Nine;
        case durak::core::Rank::Ten: return durak::gen::net::Rank::Ten;
        case durak::core::Rank::Jack: return durak::gen::net::Rank::Jack;
        case durak::core::Rank::Queen: return durak::gen::net::Rank::Queen;
        case durak::core::Rank::King: return durak::gen::net::Rank::King;
        case durak::core::Rank::Ace: return durak::gen::net::Rank::Ace;
        }
        return durak::gen::net::Rank::Two;
    }

    auto FromFbRank(durak::gen::net::Rank r) noexcept -> durak::core::Rank
    {
        switch (r)
        {
        case durak::gen::net::Rank::Two: return durak::core::Rank::Two;
        case durak::gen::net::Rank::Three: return durak::core::Rank::Three;
        case durak::gen::net::Rank::Four: return durak::core::Rank::Four;
        case durak::gen::net::Rank::Five: return durak::core::Rank::Five;
        case durak::gen::net::Rank::Six: return durak::core::Rank::Six;
        case durak::gen::net::Rank::Seven: return durak::core::Rank::Seven;
        case durak::gen::net::Rank::Eight: return durak::core::Rank::Eight;
        case durak::gen::net::Rank::Nine: return durak::core::Rank::Nine;
        case durak::gen::net::Rank::Ten: return durak::core::Rank::Ten;
        case durak::gen::net::Rank::Jack: return durak::core::Rank::Jack;
        case durak::gen::net::Rank::Queen: return durak::core::Rank::Queen;
        case durak::gen::net::Rank::King: return durak::core::Rank::King;
        case durak::gen::net::Rank::Ace: return durak::core::Rank::Ace;
        }
        return durak::core::Rank::Two;
    }

    auto ToFbPhase(durak::core::Phase p) noexcept -> durak::gen::net::Phase
    {
        switch (p)
        {
        case durak::core::Phase::Attacking: return durak::gen::net::Phase::Attacking;
        case durak::core::Phase::Defending: return durak::gen::net::Phase::Defending;
        }
        return durak::gen::net::Phase::Attacking;
    }

    auto FromFbPhase(durak::gen::net::Phase p) noexcept -> durak::core::Phase
    {
        switch (p)
        {
        case durak::gen::net::Phase::Attacking: return durak::core::Phase::Attacking;
        case durak::gen::net::Phase::Defending: return durak::core::Phase::Defending;
        }
        return durak::core::Phase::Attacking;
    }

    // Small helper that now uses the explicit mapping
    inline auto FbToSuitRank(durak::gen::net::Card const* c)
        -> std::pair<durak::core::Suit, durak::core::Rank>
    {
        return {FromFbSuit(c->suit()), FromFbRank(c->rank())};
    }
}

namespace
{
    // Verify enum layouts (one value per enum is sufficient to catch drift)
    static_assert((int)durak::core::Suit::Hearts == (int)durak::gen::net::Suit::Hearts);
    static_assert((int)durak::core::Rank::Two == (int)durak::gen::net::Rank::Two);
    static_assert((int)durak::core::Phase::Attacking == (int)durak::gen::net::Phase::Attacking);

    inline auto fb_to_sr(durak::gen::net::Card const* c)
        -> std::pair<durak::core::Suit, durak::core::Rank>
    {
        return {durak::core::net::FromFbSuit(c->suit()), durak::core::net::FromFbRank(c->rank())};
    }
} // anonymous

namespace durak::core::net
{
    static inline auto ToFbCard(flatbuffers::FlatBufferBuilder& fbb, CardVal cv)
        -> flatbuffers::Offset<durak::gen::net::Card>
    {
        // Build a Card using the current schema enums
        return durak::gen::net::CreateCard(fbb, ToFbSuit(cv.suit), ToFbRank(cv.rank));
    }

    auto BuildAction_Attack(PlyrIdxT actor,
                            std::span<const CardVal> cards,
                            std::uint64_t msg_id) -> std::vector<std::uint8_t>
    {
        flatbuffers::FlatBufferBuilder fbb;

        std::vector<flatbuffers::Offset<durak::gen::net::Card>> card_vec;
        card_vec.reserve(cards.size());
        for (CardVal cv : cards)
        {
            card_vec.push_back(ToFbCard(fbb, cv));
        }

        auto const act = durak::gen::net::CreateAction_Attack(
            fbb, actor, fbb.CreateVector(card_vec));

        auto const pam = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Attack, act.Union());

        auto const env = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());

        fbb.Finish(env);

        std::vector<std::uint8_t> out(fbb.GetSize());
        std::memcpy(out.data(), fbb.GetBufferPointer(), fbb.GetSize());
        return out;
    }

    auto BuildAction_Defend(PlyrIdxT actor,
                            std::span<const DefPair> pairs,
                            std::uint64_t msg_id) -> std::vector<std::uint8_t>
    {
        flatbuffers::FlatBufferBuilder fbb;

        std::vector<flatbuffers::Offset<durak::gen::net::ActionPair>> ap_vec;
        ap_vec.reserve(pairs.size());
        for (DefPair const& p : pairs)
        {
            auto const a = ToFbCard(fbb, p.attack);
            auto const d = ToFbCard(fbb, p.defend);
            ap_vec.push_back(durak::gen::net::CreateActionPair(fbb, a, d));
        }

        auto const def = durak::gen::net::CreateAction_Defend(
            fbb, actor, fbb.CreateVector(ap_vec));

        auto const pam = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Defend, def.Union());

        auto const env = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());

        fbb.Finish(env);

        std::vector<std::uint8_t> out(fbb.GetSize());
        std::memcpy(out.data(), fbb.GetBufferPointer(), fbb.GetSize());
        return out;
    }

    // ---------- Snapshot (server → client) ----------

    auto BuildSnapshot(durak::core::GameImpl const& g,
                       durak::core::PlyrIdxT seat,
                       std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        std::shared_ptr<durak::core::GameSnapshot const> snap = g.SnapshotFor(seat);

        flatbuffers::FlatBufferBuilder fbb;

        // Table
        std::vector<flatbuffers::Offset<durak::gen::net::TableSlot>> tbl;
        tbl.reserve(snap->table.size());
        for (durak::core::TableSlotView const& tv : snap->table)
        {
            flatbuffers::Offset<durak::gen::net::Card> a_off{};
            if (auto sp = tv.attack.lock())
            {
                a_off = durak::gen::net::CreateCard(fbb, ToFbSuit(sp->suit), ToFbRank(sp->rank));
            }

            flatbuffers::Offset<durak::gen::net::Card> d_off{};
            if (auto sp = tv.defend.lock())
            {
                d_off = durak::gen::net::CreateCard(fbb, ToFbSuit(sp->suit), ToFbRank(sp->rank));
            }

            tbl.push_back(durak::gen::net::CreateTableSlot(fbb, a_off, d_off));
        }
        auto const tbl_vec = fbb.CreateVector(tbl);

        // My hand
        std::vector<flatbuffers::Offset<durak::gen::net::Card>> my;
        my.reserve(snap->my_hand.size());
        for (durak::core::CardWP const& w : snap->my_hand)
        {
            if (auto sp = w.lock())
            {
                my.push_back(durak::gen::net::CreateCard(fbb, ToFbSuit(sp->suit), ToFbRank(sp->rank)));
            }
        }
        auto const my_vec = fbb.CreateVector(my);

        // Other counts unchanged…

        auto const view = durak::gen::net::CreateSeatView(
            fbb,
            /*schema_version*/ 1,
            /*seat*/ seat,
            /*n_players*/ static_cast<uint8_t>(snap->n_players),
            /*trump*/ ToFbSuit(snap->trump),
            /*attacker_idx*/ snap->attacker_idx,
            /*defender_idx*/ snap->defender_idx,
            /*phase*/ ToFbPhase(snap->phase),
            /*table*/ tbl_vec,
            /*my_hand*/ my_vec,
            /*other_counts*/ fbb.CreateVector(snap->other_counts),
            /*bout_cap*/ snap->bout_cap,
            /*attacks_used*/ snap->attacks_used,
            /*defender_took*/ snap->defender_took
        );

        auto const sm = durak::gen::net::CreateSnapshotMsg(fbb, msg_id, view);
        auto const env = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::SnapshotMsg, sm.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    // ---------- Violation (server → client) ----------

    auto BuildViolation(durak::core::error::RuleViolation const& v,
                        std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        flatbuffers::FlatBufferBuilder fbb;
        auto const txt = fbb.CreateString(durak::core::error::describe(v));
        auto const vio = durak::gen::net::CreateViolation(
            fbb, msg_id, static_cast<int16_t>(v.code), txt);
        auto const env = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::Violation, vio.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    // ---------- Builders (client → server) ----------

    auto BuildAction_Attack(durak::core::PlyrIdxT actor,
                            std::span<durak::core::CardWP const> cards,
                            std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        flatbuffers::FlatBufferBuilder fbb;

        std::vector<flatbuffers::Offset<durak::gen::net::Card>> vec;
        vec.reserve(cards.size());
        for (durak::core::CardWP const& w : cards)
            if (auto sp = w.lock())
                vec.push_back(durak::gen::net::CreateCard(fbb, ToFbSuit(sp->suit), ToFbRank(sp->rank)));

        auto const a = durak::gen::net::CreateAction_Attack(fbb, actor, fbb.CreateVector(vec));
        auto const m = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Attack, a.Union());
        auto const e = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, m.Union());
        fbb.Finish(e);
        return fbb.Release();
    }

    auto BuildAction_Defend(durak::core::PlyrIdxT actor,
                            std::span<durak::core::DefendPair const> pairs,
                            std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        flatbuffers::FlatBufferBuilder fbb;

        std::vector<flatbuffers::Offset<durak::gen::net::ActionPair>> vec;
        vec.reserve(pairs.size());
        for (durak::core::DefendPair const& p : pairs)
        {
            auto atk = p.attack.lock();
            auto def = p.defend.lock();
            if (!atk || !def) continue; // skip invalid
            auto const a = durak::gen::net::CreateCard(fbb, ToFbSuit(atk->suit), ToFbRank(atk->rank));
            auto const d = durak::gen::net::CreateCard(fbb, ToFbSuit(def->suit), ToFbRank(def->rank));
            vec.push_back(durak::gen::net::CreateActionPair(fbb, a, d));
        }

        auto const dmsg = durak::gen::net::CreateAction_Defend(fbb, actor, fbb.CreateVector(vec));
        auto const m = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Defend, dmsg.Union());
        auto const e = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, m.Union());
        fbb.Finish(e);
        return fbb.Release();
    }

    auto BuildAction_Pass(durak::core::PlyrIdxT actor,
                          std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        flatbuffers::FlatBufferBuilder fbb;
        auto const p = durak::gen::net::CreateAction_Pass(fbb, actor);
        auto const m = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Pass, p.Union());
        auto const e = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, m.Union());
        fbb.Finish(e);
        return fbb.Release();
    }

    auto BuildAction_Take(durak::core::PlyrIdxT actor,
                          std::uint64_t msg_id)
        -> flatbuffers::DetachedBuffer
    {
        flatbuffers::FlatBufferBuilder fbb;
        auto const t = durak::gen::net::CreateAction_Take(fbb, actor);
        auto const m = durak::gen::net::CreatePlayerActionMsg(
            fbb, msg_id, durak::gen::net::Action::Action_Take, t.Union());
        auto const e = durak::gen::net::CreateEnvelope(
            fbb, durak::gen::net::Message::PlayerActionMsg, m.Union());
        fbb.Finish(e);
        return fbb.Release();
    }

    // ---------- Decode (client/server ← inbound wire) ----------

    auto DecodePlayerAction(durak::core::GameImpl& g,
                            std::span<std::byte const> bytes)
        -> std::expected<DecodedAction, ParseError>
    {
        if (bytes.size() < sizeof(flatbuffers::uoffset_t))
            return std::unexpected(ParseError{"buffer too small"});

        auto const* env = durak::gen::net::GetEnvelope(
            reinterpret_cast<uint8_t const*>(bytes.data()));
        if (!env)
            return std::unexpected(ParseError{"bad root"});

        if (env->message_type() != durak::gen::net::Message::PlayerActionMsg)
            return std::unexpected(ParseError{"not a PlayerActionMsg"});

        auto const* pam = env->message_as_PlayerActionMsg();
        auto const kind = pam->action_type();

        DecodedAction out{};

        switch (kind)
        {
        case durak::gen::net::Action::Action_Attack:
        {
            auto const* a = pam->action_as_Action_Attack();
            DecodedAction out{};
            out.actor = static_cast<durak::core::PlyrIdxT>(a->actor());

            std::vector<durak::core::CardWP> w;
            if (auto const* v = a->cards())
            {
                w.reserve(v->size());
                for (auto const* fb_c : *v)
                {
                    auto const [s, r] = FbToSuitRank(fb_c);
                    durak::core::Card probe{s, r};
                    w.push_back(g.FindFromHand(out.actor, probe));
                }
            }
            out.action = durak::core::AttackAction{std::move(w)};
            return out;
        }

        case durak::gen::net::Action::Action_Defend:
        {
            auto const* d = pam->action_as_Action_Defend();
            DecodedAction out{};
            out.actor = static_cast<durak::core::PlyrIdxT>(d->actor());

            std::vector<durak::core::DefendPair> pairs;
            if (auto const* v = d->pairs())
            {
                pairs.reserve(v->size());
                for (auto const* fb_p : *v)
                {
                    auto const [sa, ra] = FbToSuitRank(fb_p->attack());
                    auto const [sd, rd] = FbToSuitRank(fb_p->defend());

                    durak::core::Card atk{sa, ra};
                    durak::core::Card def{sd, rd};

                    pairs.push_back(durak::core::DefendPair{
                        .attack = g.FindFromAtkTable(atk),
                        .defend = g.FindFromHand(out.actor, def)
                    });
                }
            }
            out.action = durak::core::DefendAction{std::move(pairs)};
            return out;
        }

        case durak::gen::net::Action::Action_Pass:
        {
            auto const* p = pam->action_as_Action_Pass();
            out.actor = static_cast<durak::core::PlyrIdxT>(p->actor());
            out.action = durak::core::PassAction{};
            return out;
        }

        case durak::gen::net::Action::Action_Take:
        {
            auto const* t = pam->action_as_Action_Take();
            out.actor = static_cast<durak::core::PlyrIdxT>(t->actor());
            out.action = durak::core::TakeAction{};
            return out;
        }

        default:
            return std::unexpected(ParseError{"unknown action variant"});
        }
    }
} // namespace durak::core::net
