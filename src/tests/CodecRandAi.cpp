#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <span>
#include <vector>
#include <expected>
#include <cstddef>

#include "../core/Types.hpp"
#include "../core/Actions.hpp"
#include "../core/State.hpp"
#include "../core/Rules.hpp"
#include "../core/ClassicRules.hpp"
#include "../core/Game.hpp"
#include "../core/RandomAi.hpp"
#include "../core/Exception.hpp"

#include "../net/Codec.hpp"  // BuildSnapshot + DecodePlayerAction
#include "../generated/flatbuffers/durak_net_generated.h"

using namespace durak::core;
using durak::core::net::BuildSnapshot;
using durak::core::net::DecodePlayerAction;

namespace
{
    // ---------- tiny helpers ----------
    struct Seeds
    {
        uint64_t game_seed;
        uint64_t p0_seed;
        uint64_t p1_seed;
    };

    inline GameImpl MakeGameWithRandomAIs(Seeds s, uint32_t n_players = 2)
    {
        Config cfg{};
        cfg.n_players = n_players;
        cfg.deal_up_to = 6;
        cfg.deck36 = true;
        cfg.seed = s.game_seed;

        std::vector<std::unique_ptr<Player>> players;
        players.reserve(n_players);
        players.emplace_back(std::make_unique<RandomAI>(s.p0_seed));
        players.emplace_back(std::make_unique<RandomAI>(s.p1_seed));

        return GameImpl(cfg, std::make_unique<ClassicRules>(), std::move(players));
    }

    inline void AdvanceNSteps(GameImpl& g, int steps)
    {
        for (int i = 0; i < steps; ++i) (void)g.Step();
    }

    inline std::span<const std::byte> AsBytes(const flatbuffers::DetachedBuffer& buf)
    {
        const uint8_t* p = buf.data();
        return {reinterpret_cast<const std::byte*>(p), buf.size()};
    }

    // FB enum casts (layouts are asserted equal inside codec.cpp)
    inline durak::gen::net::Suit to_fb(Suit s) { return static_cast<durak::gen::net::Suit>(s); }
    inline durak::gen::net::Rank to_fb(Rank r) { return static_cast<durak::gen::net::Rank>(r); }

    // Attack(actor, [cards...]) – manual FB build
    inline flatbuffers::DetachedBuffer MakeAttackFB(PlyrIdxT actor, std::span<const Card> cards, uint64_t msg_id)
    {
        flatbuffers::FlatBufferBuilder fbb;
        std::vector<flatbuffers::Offset<durak::gen::net::Card>> v;
        v.reserve(cards.size());
        for (Card const& c : cards)
        {
            v.push_back(durak::gen::net::CreateCard(fbb, to_fb(c.suit), to_fb(c.rank)));
        }
        flatbuffers::Offset<durak::gen::net::Action_Attack> act =
            durak::gen::net::CreateAction_Attack(fbb, actor, fbb.CreateVector(v));
        flatbuffers::Offset<durak::gen::net::PlayerActionMsg> pam =
            durak::gen::net::CreatePlayerActionMsg(fbb, msg_id,
                                                   durak::gen::net::Action::Action_Attack, act.Union());
        flatbuffers::Offset<durak::gen::net::Envelope> env =
            durak::gen::net::CreateEnvelope(fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    // Defend(actor, [{attack,defend}, ...]) – pass by value (Suit/Rank)
    struct DefPairVal
    {
        Card attack;
        Card defend;
    };

    inline flatbuffers::DetachedBuffer MakeDefendFB(PlyrIdxT actor, std::span<const DefPairVal> pairs, uint64_t msg_id)
    {
        flatbuffers::FlatBufferBuilder fbb;
        std::vector<flatbuffers::Offset<durak::gen::net::ActionPair>> v;
        v.reserve(pairs.size());
        for (DefPairVal const& p : pairs)
        {
            flatbuffers::Offset<durak::gen::net::Card> a =
                durak::gen::net::CreateCard(fbb, to_fb(p.attack.suit), to_fb(p.attack.rank));
            flatbuffers::Offset<durak::gen::net::Card> d =
                durak::gen::net::CreateCard(fbb, to_fb(p.defend.suit), to_fb(p.defend.rank));
            v.push_back(durak::gen::net::CreateActionPair(fbb, a, d));
        }
        flatbuffers::Offset<durak::gen::net::Action_Defend> def =
            durak::gen::net::CreateAction_Defend(fbb, actor, fbb.CreateVector(v));
        flatbuffers::Offset<durak::gen::net::PlayerActionMsg> pam =
            durak::gen::net::CreatePlayerActionMsg(fbb, msg_id,
                                                   durak::gen::net::Action::Action_Defend, def.Union());
        flatbuffers::Offset<durak::gen::net::Envelope> env =
            durak::gen::net::CreateEnvelope(fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    inline flatbuffers::DetachedBuffer MakePassFB(PlyrIdxT actor, uint64_t msg_id)
    {
        flatbuffers::FlatBufferBuilder fbb;
        flatbuffers::Offset<durak::gen::net::Action_Pass> p =
            durak::gen::net::CreateAction_Pass(fbb, actor);
        flatbuffers::Offset<durak::gen::net::PlayerActionMsg> pam =
            durak::gen::net::CreatePlayerActionMsg(fbb, msg_id,
                                                   durak::gen::net::Action::Action_Pass, p.Union());
        flatbuffers::Offset<durak::gen::net::Envelope> env =
            durak::gen::net::CreateEnvelope(fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    inline flatbuffers::DetachedBuffer MakeTakeFB(PlyrIdxT actor, uint64_t msg_id)
    {
        flatbuffers::FlatBufferBuilder fbb;
        flatbuffers::Offset<durak::gen::net::Action_Take> t =
            durak::gen::net::CreateAction_Take(fbb, actor);
        flatbuffers::Offset<durak::gen::net::PlayerActionMsg> pam =
            durak::gen::net::CreatePlayerActionMsg(fbb, msg_id,
                                                   durak::gen::net::Action::Action_Take, t.Union());
        flatbuffers::Offset<durak::gen::net::Envelope> env =
            durak::gen::net::CreateEnvelope(fbb, durak::gen::net::Message::PlayerActionMsg, pam.Union());
        fbb.Finish(env);
        return fbb.Release();
    }

    // Robustly search forward for a state where defender has a legal cover for an uncovered attack.
    inline bool FindDefendOpportunity(GameImpl& g,
                                      int max_steps,
                                      PlyrIdxT& def_out,
                                      CardSP& atk_card_out,
                                      CardSP& def_card_out)
    {
        for (int i = 0; i < max_steps; ++i)
        {
            if (g.PhaseNow() == Phase::Defending)
            {
                PlyrIdxT def = g.Defender();
                std::shared_ptr<const GameSnapshot> s = g.SnapshotFor(def);

                // find any uncovered attack
                for (size_t i_slot = 0; i_slot < s->table.size(); ++i_slot)
                {
                    CardWP atk_wp = s->table[i_slot].attack;
                    CardWP def_wp = s->table[i_slot].defend;
                    if (atk_wp.expired() || !def_wp.expired()) continue;

                    CardSP atk = atk_wp.lock();
                    if (!atk) continue;

                    // find any defender card that beats it
                    for (CardWP w : s->my_hand)
                    {
                        CardSP cand = w.lock();
                        if (!cand) continue;
                        if (ClassicRules::Beats(*cand, *atk, s->trump))
                        {
                            def_out = def;
                            atk_card_out = std::move(atk);
                            def_card_out = std::move(cand);
                            return true;
                        }
                    }
                }
            }
            (void)g.Step();
        }
        return false;
    }
} // namespace

// ================== TESTS ==================

TEST(Codec_RandomAI, EncodeDecode_Attack_RoundTrip_FromLiveSnapshot)
{
    std::array<Seeds, 2> scenarios{
        {
            {0xA11CE5EEDULL, 0xBEEF'0001ULL, 0xBEEF'0002ULL},
            {0xF00D'F00DULL, 0xDEAD'1234ULL, 0xBADC'0DE0ULL}
        }
    };

    for (Seeds const& seeds : scenarios)
    {
        GameImpl game = MakeGameWithRandomAIs(seeds);
        AdvanceNSteps(game, /*steps*/7);

        const PlyrIdxT atk = game.Attacker();
        std::shared_ptr<const GameSnapshot> snap = game.SnapshotFor(atk);
        ASSERT_GE(snap->my_hand.size(), 2u);

        CardSP c0 = snap->my_hand[0].lock();
        CardSP c1 = snap->my_hand[1].lock();
        ASSERT_TRUE(c0 && c1);

        // Build FB from value-cards; Decode should resolve to live pointers via FindFromHand
        std::array<Card, 2> chosen = {Card{c0->suit, c0->rank}, Card{c1->suit, c1->rank}};
        flatbuffers::DetachedBuffer buf =
            MakeAttackFB(atk, std::span<const Card>{chosen.data(), chosen.size()}, /*msg_id*/42);

        std::expected<durak::core::net::DecodedAction, durak::core::net::ParseError> res =
            DecodePlayerAction(game, AsBytes(buf));
        ASSERT_TRUE(res.has_value());

        durak::core::net::DecodedAction out = std::move(res.value());
        EXPECT_EQ(out.actor, atk);
        ASSERT_TRUE(std::holds_alternative<AttackAction>(out.action));

        AttackAction const& aa = std::get<AttackAction>(out.action);
        ASSERT_EQ(aa.cards.size(), chosen.size());

        // Resolved weak_ptrs should alias the exact shared_ptrs in the live hand
        CardSP r0 = aa.cards[0].lock();
        CardSP r1 = aa.cards[1].lock();
        ASSERT_TRUE(r0 && r1);
        EXPECT_EQ(r0.get(), c0.get());
        EXPECT_EQ(r1.get(), c1.get());
    }
}

TEST(Codec_RandomAI, Decode_Defend_FromLiveDefendingSnapshot)
{
    // Try a couple of deterministic scenarios and search forward for a coverable attack.
    std::array<Seeds, 2> scenarios{
        {
            {0x5EED5EEDULL, 0x11112222ULL, 0x33334444ULL},
            {0xAABBCCDDULL, 0x01020304ULL, 0x05060708ULL}
        }
    };

    bool validated_one = false;

    for (Seeds const& seeds : scenarios)
    {
        GameImpl game = MakeGameWithRandomAIs(seeds);

        PlyrIdxT def{};
        CardSP atk_card{};
        CardSP def_card{};

        if (!FindDefendOpportunity(game, /*max_steps*/500, def, atk_card, def_card))
        {
            continue; // try next scenario
        }

        ASSERT_TRUE(atk_card && def_card);

        // Build Defend FB from value-cards
        DefPairVal pv{Card{atk_card->suit, atk_card->rank}, Card{def_card->suit, def_card->rank}};
        flatbuffers::DetachedBuffer buf =
            MakeDefendFB(def, std::span<const DefPairVal>(&pv, 1), /*msg_id*/999);

        std::expected<durak::core::net::DecodedAction, durak::core::net::ParseError> res =
            DecodePlayerAction(game, AsBytes(buf));
        ASSERT_TRUE(res.has_value());

        durak::core::net::DecodedAction out = std::move(res.value());
        EXPECT_EQ(out.actor, def);
        ASSERT_TRUE(std::holds_alternative<DefendAction>(out.action));

        DefendAction const& da = std::get<DefendAction>(out.action);
        ASSERT_EQ(da.pairs.size(), 1u);

        CardSP atk_out = da.pairs[0].attack.lock();
        CardSP def_out = da.pairs[0].defend.lock();
        ASSERT_TRUE(atk_out && def_out);
        EXPECT_EQ(atk_out.get(), atk_card.get());
        EXPECT_EQ(def_out.get(), def_card.get());

        validated_one = true;
        break; // one solid success is enough
    }

    ASSERT_TRUE(validated_one) <<
        "Could not locate a defendable uncovered attack within search budget; try adjusting seeds or steps.";
}

TEST(Codec_RandomAI, EncodeDecode_Pass_Take_FromLiveSnapshots)
{
    GameImpl game = MakeGameWithRandomAIs({0xCAFEFACEULL, 0x12345678ULL, 0x87654321ULL});
    AdvanceNSteps(game, 5);

    const PlyrIdxT atk = game.Attacker();
    const PlyrIdxT def = game.Defender();

    {
        // Pass (attacker)
        flatbuffers::DetachedBuffer buf = MakePassFB(atk, /*msg_id*/101);
        std::expected<durak::core::net::DecodedAction, durak::core::net::ParseError> res =
            DecodePlayerAction(game, AsBytes(buf));
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res->actor, atk);
        EXPECT_TRUE(std::holds_alternative<PassAction>(res->action));
    }

    {
        // Take (defender)
        flatbuffers::DetachedBuffer buf = MakeTakeFB(def, /*msg_id*/102);
        std::expected<durak::core::net::DecodedAction, durak::core::net::ParseError> res =
            DecodePlayerAction(game, AsBytes(buf));
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res->actor, def);
        EXPECT_TRUE(std::holds_alternative<TakeAction>(res->action));
    }
}

TEST(Codec_RandomAI, BuildSnapshot_MatchesAuthoritativeState)
{
    GameImpl game = MakeGameWithRandomAIs({0x0BADC0DEULL, 0xFEEDFACEULL, 0xC001D00DULL});
    AdvanceNSteps(game, 9);

    const PlyrIdxT seat = game.Attacker();
    std::shared_ptr<const GameSnapshot> live = game.SnapshotFor(seat);

    flatbuffers::DetachedBuffer buf = BuildSnapshot(game, seat, /*msg_id*/2024);
    durak::gen::net::Envelope const* env = durak::gen::net::GetEnvelope(buf.data());
    ASSERT_NE(env, nullptr);
    ASSERT_EQ(env->message_type(), durak::gen::net::Message::SnapshotMsg);

    durak::gen::net::SnapshotMsg const* sm = env->message_as_SnapshotMsg();
    ASSERT_NE(sm, nullptr);
    durak::gen::net::SeatView const* sv = sm->view();
    ASSERT_NE(sv, nullptr);

    EXPECT_EQ(sv->seat(), seat);
    EXPECT_EQ(sv->n_players(), static_cast<uint8_t>(live->n_players));
    EXPECT_EQ(static_cast<int>(sv->trump()), static_cast<int>(live->trump));
    EXPECT_EQ(sv->attacker_idx(), live->attacker_idx);
    EXPECT_EQ(sv->defender_idx(), live->defender_idx);
    EXPECT_EQ(static_cast<int>(sv->phase()), static_cast<int>(live->phase));
    EXPECT_EQ(sv->bout_cap(), live->bout_cap);
    EXPECT_EQ(sv->attacks_used(), live->attacks_used);
    EXPECT_EQ(sv->defender_took(), live->defender_took);

    ASSERT_NE(sv->my_hand(), nullptr);
    EXPECT_EQ(sv->my_hand()->size(), live->my_hand.size());

    ASSERT_NE(sv->other_counts(), nullptr);
    ASSERT_EQ(sv->other_counts()->size(), live->other_counts.size());
    for (size_t i = 0; i < live->other_counts.size(); ++i)
        EXPECT_EQ((*sv->other_counts())[i], live->other_counts[i]);

    ASSERT_NE(sv->table(), nullptr);
    ASSERT_EQ(sv->table()->size(), constants::MaxTableSlots);

    for (size_t i = 0; i < sv->table()->size(); ++i)
    {
        durak::gen::net::TableSlot const* fb = sv->table()->Get(i);
        TableSlotView const& tv = live->table[i];

        bool atk_present = !tv.attack.expired();
        bool def_present = !tv.defend.expired();

        if (atk_present)
        {
            CardSP a = tv.attack.lock();
            ASSERT_NE(fb->attack(), nullptr);
            EXPECT_EQ(static_cast<int>(fb->attack()->suit()), static_cast<int>(a->suit));
            EXPECT_EQ(static_cast<int>(fb->attack()->rank()), static_cast<int>(a->rank));
        }
        else
        {
            EXPECT_EQ(fb->attack(), nullptr);
        }

        if (def_present)
        {
            CardSP d = tv.defend.lock();
            ASSERT_NE(fb->defend(), nullptr);
            EXPECT_EQ(static_cast<int>(fb->defend()->suit()), static_cast<int>(d->suit));
            EXPECT_EQ(static_cast<int>(fb->defend()->rank()), static_cast<int>(d->rank));
        }
        else
        {
            EXPECT_EQ(fb->defend(), nullptr);
        }
    }
}