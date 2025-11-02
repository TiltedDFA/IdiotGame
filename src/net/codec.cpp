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

namespace
{
    // Verify enum layouts (one value per enum is sufficient to catch drift)
    static_assert((int)durak::core::Suit::Hearts == (int)durak::gen::net::Suit::Hearts);
    static_assert((int)durak::core::Rank::Two == (int)durak::gen::net::Rank::Two);
    static_assert((int)durak::core::Phase::Attacking == (int)durak::gen::net::Phase::Attacking);

    inline auto to_fb(durak::core::Suit s) -> durak::gen::net::Suit { return static_cast<durak::gen::net::Suit>(s); }
    inline auto to_fb(durak::core::Rank r) -> durak::gen::net::Rank { return static_cast<durak::gen::net::Rank>(r); }
    inline auto to_fb(durak::core::Phase p) -> durak::gen::net::Phase { return static_cast<durak::gen::net::Phase>(p); }

    inline auto from_fb(durak::gen::net::Suit s) -> durak::core::Suit { return static_cast<durak::core::Suit>(s); }
    inline auto from_fb(durak::gen::net::Rank r) -> durak::core::Rank { return static_cast<durak::core::Rank>(r); }
    inline auto from_fb(durak::gen::net::Phase p) -> durak::core::Phase { return static_cast<durak::core::Phase>(p); }

    inline auto fb_to_sr(durak::gen::net::Card const* c)
        -> std::pair<durak::core::Suit, durak::core::Rank>
    {
        return {from_fb(c->suit()), from_fb(c->rank())};
    }
} // anonymous

namespace durak::core::net
{
    static inline auto ToFbCard(flatbuffers::FlatBufferBuilder& fbb, CardVal cv)
        -> flatbuffers::Offset<durak::gen::net::Card>
    {
        // Build a Card using the current schema enums
        return durak::gen::net::CreateCard(fbb, to_fb(cv.suit), to_fb(cv.rank));
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

        // Table → FB
        std::vector<flatbuffers::Offset<durak::gen::net::TableSlot>> tbl;
        tbl.reserve(snap->table.size());
        for (durak::core::TableSlotView const& tv : snap->table)
        {
            flatbuffers::Offset<durak::gen::net::Card> a_off{};
            if (auto sp = tv.attack.lock())
                a_off = durak::gen::net::CreateCard(fbb, to_fb(sp->suit), to_fb(sp->rank));

            flatbuffers::Offset<durak::gen::net::Card> d_off{};
            if (auto sp = tv.defend.lock())
                d_off = durak::gen::net::CreateCard(fbb, to_fb(sp->suit), to_fb(sp->rank));

            tbl.push_back(durak::gen::net::CreateTableSlot(fbb, a_off, d_off));
        }
        auto const tbl_vec = fbb.CreateVector(tbl);

        // My hand → FB
        std::vector<flatbuffers::Offset<durak::gen::net::Card>> my;
        my.reserve(snap->my_hand.size());
        for (durak::core::CardWP const& w : snap->my_hand)
            if (auto sp = w.lock())
                my.push_back(durak::gen::net::CreateCard(fbb, to_fb(sp->suit), to_fb(sp->rank)));
        auto const my_vec = fbb.CreateVector(my);

        // Other counts
        auto const cnt_vec = fbb.CreateVector(snap->other_counts);

        // SeatView
        auto const view = durak::gen::net::CreateSeatView(
            fbb,
            /*schema_version*/ 1,
            /*seat*/ seat,
            /*n_players*/ static_cast<uint8_t>(snap->n_players),
            /*trump*/ to_fb(snap->trump),
            /*attacker_idx*/ snap->attacker_idx,
            /*defender_idx*/ snap->defender_idx,
            /*phase*/ to_fb(snap->phase),
            /*table*/ tbl_vec,
            /*my_hand*/ my_vec,
            /*other_counts*/ cnt_vec,
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
                vec.push_back(durak::gen::net::CreateCard(fbb, to_fb(sp->suit), to_fb(sp->rank)));

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
            auto const a = durak::gen::net::CreateCard(fbb, to_fb(atk->suit), to_fb(atk->rank));
            auto const d = durak::gen::net::CreateCard(fbb, to_fb(def->suit), to_fb(def->rank));
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
            out.actor = static_cast<durak::core::PlyrIdxT>(a->actor());

            std::vector<durak::core::CardWP> w;
            if (auto const* v = a->cards())
            {
                w.reserve(v->size());
                for (auto const* fb_c : *v)
                {
                    auto const [s, r] = fb_to_sr(fb_c);
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
            out.actor = static_cast<durak::core::PlyrIdxT>(d->actor());

            std::vector<durak::core::DefendPair> pairs;
            if (auto const* v = d->pairs())
            {
                pairs.reserve(v->size());
                for (auto const* fb_p : *v)
                {
                    auto const [sa, ra] = fb_to_sr(fb_p->attack());
                    auto const [sd, rd] = fb_to_sr(fb_p->defend());

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
