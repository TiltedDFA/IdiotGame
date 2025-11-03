// File: src/apps/NetAIClientMain.cpp
//
// Allman braces. Explicit types. High-verbosity logs.
//
// A headless client that plays via RandomAI. Connects to the server,
// reads SnapshotMsg frames, chooses an action, and sends PlayerActionMsg.
//

#include <cstdint>
#include <print>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "core/Types.hpp"
#include "core/State.hpp"
#include "core/Actions.hpp"
#include "core/RandomAi.hpp"
#include "net/codec.hpp"

#include "generated/flatbuffers/durak_net_generated.h"

namespace
{
    using WsClient = websocketpp::client<websocketpp::config::asio_client>;

    // Helpers to reconstruct a transient GameSnapshot from FB SeatView.
    // We allocate local shared_ptr<Card> owners so that CardWP/weak_ptr can lock.
    struct SnapshotScratch
    {
        // Owners that must outlive the GameSnapshot
        std::vector<durak::core::CardSP> my_owners;
        std::array<durak::core::CardSP, durak::core::constants::MaxTableSlots> atk_owners;
        std::array<durak::core::CardSP, durak::core::constants::MaxTableSlots> def_owners;
    };

    // Converts a SeatView into a core::GameSnapshot + owning scratch.
    // The returned snapshot references memory in 'scratch' via weak_ptrs.
    durak::core::GameSnapshot to_snapshot(const durak::gen::net::SeatView* sv,
                                          SnapshotScratch& scratch_out)
    {
        durak::core::GameSnapshot gs{};

        // NEW: make sure the AI knows which seat this view is for.
        // Pick the correct field name for your GameSnapshot:
        //   - if it is 'seat', use gs.seat
        //   - if it is 'my_seat' or 'viewer_idx', use that instead.
        // gs. = sv->seat();  // <-- adjust the member name if different

        gs.trump = durak::core::net::FromFbSuit(sv->trump());
        gs.n_players = sv->n_players();
        gs.attacker_idx = sv->attacker_idx();
        gs.defender_idx = sv->defender_idx();
        gs.phase = durak::core::net::FromFbPhase(sv->phase());

        // Table
        durak::core::TableViewT table{};
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            table[i] = durak::core::TableSlotView{};
        }
        if (const flatbuffers::Vector<flatbuffers::Offset<durak::gen::net::TableSlot>>* tbl = sv->table())
        {
            for (std::size_t i = 0; i < static_cast<std::size_t>(tbl->size()) &&
                                   i < durak::core::constants::MaxTableSlots; ++i)
            {
                const durak::gen::net::TableSlot* ts = tbl->Get(static_cast<flatbuffers::uoffset_t>(i));
                if (ts->attack() != nullptr)
                {
                    durak::core::CardSP sp = std::make_shared<durak::core::Card>(
                        durak::core::net::FromFbSuit(ts->attack()->suit()),
                        durak::core::net::FromFbRank(ts->attack()->rank())
                    );
                    scratch_out.atk_owners[i] = sp;
                    table[i].attack = durak::core::CardWP{sp};
                }
                if (ts->defend() != nullptr)
                {
                    durak::core::CardSP sp = std::make_shared<durak::core::Card>(
                        durak::core::net::FromFbSuit(ts->defend()->suit()),
                        durak::core::net::FromFbRank(ts->defend()->rank())
                    );
                    scratch_out.def_owners[i] = sp;
                    table[i].defend = durak::core::CardWP{sp};
                }
            }
        }
        gs.table = table;

        // My hand
        scratch_out.my_owners.clear();
        if (const flatbuffers::Vector<flatbuffers::Offset<durak::gen::net::Card>>* hv = sv->my_hand())
        {
            scratch_out.my_owners.reserve(hv->size());
            for (flatbuffers::uoffset_t i = 0; i < hv->size(); ++i)
            {
                const durak::gen::net::Card* c = hv->Get(i);
                durak::core::CardSP sp = std::make_shared<durak::core::Card>(
                    durak::core::net::FromFbSuit(c->suit()), durak::core::net::FromFbRank(c->rank()));
                scratch_out.my_owners.push_back(sp);
                gs.my_hand.push_back(durak::core::CardWP{sp});
            }
        }

        // Other counts
        gs.other_counts.clear();
        if (const flatbuffers::Vector<std::uint8_t>* oc = sv->other_counts())
        {
            for (flatbuffers::uoffset_t i = 0; i < oc->size(); ++i)
            {
                gs.other_counts.push_back(oc->Get(i));
            }
        }

        gs.bout_cap = sv->bout_cap();
        gs.attacks_used = sv->attacks_used();
        gs.defender_took = sv->defender_took();

        return gs;
    }

    static std::array<bool, 16> table_rank_mask(const durak::gen::net::SeatView* sv)
    {
        std::array<bool, 16> have{}; // Rank is <= 14 in classic decks; 16 is safe headroom
        if (const auto* tbl = sv->table())
        {
            for (flatbuffers::uoffset_t i = 0; i < tbl->size(); ++i)
            {
                const auto* ts = tbl->Get(i);
                if (const auto* a = ts->attack()) { have[static_cast<int>(a->rank())] = true; }
                if (const auto* d = ts->defend()) { have[static_cast<int>(d->rank())] = true; }
            }
        }
        return have;
    }

    // Hash the "turn state" so we only send once per turn state
    static std::uint64_t make_turn_key(const durak::gen::net::SeatView* sv)
    {
        // Phase (8) | attacker (8) | defender (8) | attacks_used (8) | defender_took (1) | bout_cap (8)
        std::uint64_t key = 0;
        key |= (static_cast<std::uint64_t>(sv->phase())        & 0xFFu) << 40;
        key |= (static_cast<std::uint64_t>(sv->attacker_idx()) & 0xFFu) << 32;
        key |= (static_cast<std::uint64_t>(sv->defender_idx()) & 0xFFu) << 24;
        key |= (static_cast<std::uint64_t>(sv->attacks_used()) & 0xFFu) << 16;
        key |= (static_cast<std::uint64_t>(sv->defender_took()) & 0x1u) << 8;
        key |= (static_cast<std::uint64_t>(sv->bout_cap()) & 0xFFu);
        return key;
    }

    // Convert a RandomAI-produced PlayerAction (which contains weak_ptrs) into
    // value forms that codec builders expect.
    bool build_outbound_action(durak::core::PlayerAction const& act,
                           durak::core::PlyrIdxT actor,
                           std::vector<std::uint8_t>& out_bytes)
{
    using namespace durak::core;
    using durak::core::net::CardVal;
    using durak::core::net::DefPair;

    auto copy_from_detached = [&](flatbuffers::DetachedBuffer&& buf)
    {
        out_bytes.assign(buf.data(), buf.data() + buf.size());
    };

    return std::visit([&](auto const& a) -> bool
    {
        using T = std::decay_t<decltype(a)>;

        if constexpr (std::is_same_v<T, AttackAction>)
        {
            std::vector<CardVal> vals;
            vals.reserve(a.cards.size());

            for (CardWP const& w : a.cards)
            {
                CCardSP sp = w.lock();
                if (!sp)
                {
                    return false; // snapshot card owner already gone
                }
                vals.push_back(CardVal{ sp->suit, sp->rank });
            }

            out_bytes = durak::core::net::BuildAction_Attack(
                actor,
                std::span<const CardVal>(vals.data(), vals.size()),
                /*msg_id*/ 777
            );
            return true;
        }
        else if constexpr (std::is_same_v<T, DefendAction>)
        {
            std::vector<DefPair> vals;
            vals.reserve(a.pairs.size());

            for (DefendPair const& p : a.pairs)
            {
                CCardSP atk = p.attack.lock();
                CCardSP def = p.defend.lock();
                if (!atk || !def)
                {
                    return false;
                }
                vals.push_back(DefPair{
                    .attack = CardVal{ atk->suit, atk->rank },
                    .defend = CardVal{ def->suit, def->rank }
                });
            }

            out_bytes = durak::core::net::BuildAction_Defend(
                actor,
                std::span<const DefPair>(vals.data(), vals.size()),
                /*msg_id*/ 778
            );
            return true;
        }
        else if constexpr (std::is_same_v<T, PassAction>)
        {
            // Use existing server/client builder and copy into a vector
            copy_from_detached(durak::core::net::BuildAction_Pass(actor, /*msg_id*/ 779));
            return true;
        }
        else if constexpr (std::is_same_v<T, TakeAction>)
        {
            copy_from_detached(durak::core::net::BuildAction_Take(actor, /*msg_id*/ 780));
            return true;
        }

        return false;
    }, act);
}

    struct CmdLine
    {
        std::string url {"ws://127.0.0.1:9002"};
        std::uint64_t seed {424242ULL};
    };

    CmdLine parse_args(int argc, char** argv)
    {
        CmdLine c{};
        for (int i = 1; i < argc; ++i)
        {
            std::string k = argv[i];
            if (k == "--url" && i + 1 < argc)
            {
                c.url = argv[++i];
            }
            else if (k == "--seed" && i + 1 < argc)
            {
                c.seed = std::strtoull(argv[++i], nullptr, 10);
            }
        }
        return c;
    }

} // anon

int main(int argc, char** argv)
{
    CmdLine cfg = parse_args(argc, argv);
    std::print("[NetAI] Connecting to {} | seed={}\n", cfg.url, cfg.seed);

    WsClient c;
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.init_asio();

    std::shared_ptr<websocketpp::connection_hdl> hdl_ptr = std::make_shared<websocketpp::connection_hdl>();
    std::atomic<bool> opened {false};

    durak::core::RandomAI ai(cfg.seed);

    // Message handler
    c.set_message_handler([&](websocketpp::connection_hdl hdl, WsClient::message_ptr msg)
{
    if (msg->get_opcode() != websocketpp::frame::opcode::binary)
    {
        std::print("[NetAI] Ignoring non-binary frame\n");
        return;
    }

    const std::string& pl = msg->get_payload();
    const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(pl.data());

    const durak::gen::net::Envelope* env = durak::gen::net::GetEnvelope(bytes);
    if (env == nullptr)
    {
        std::print("[NetAI] Bad Envelope root\n");
        return;
    }

    if (env->message_type() != durak::gen::net::Message::SnapshotMsg)
    {
        std::print("[NetAI] Non-snapshot message ignored (type={})\n",
                   static_cast<int>(env->message_type()));
        return;
    }

    const durak::gen::net::SnapshotMsg* sm = env->message_as_SnapshotMsg();
    const durak::gen::net::SeatView* sv = sm->view();
    if (sv == nullptr)
    {
        std::print("[NetAI] Snapshot missing SeatView\n");
        return;
    }

    const std::uint8_t seat = sv->seat();
    std::print("[NetAI] Snapshot: seat={} nP={} atk={} def={} phase={} attacks_used={} cap={}\n",
               static_cast<int>(sv->seat()),
               static_cast<int>(sv->n_players()),
               static_cast<int>(sv->attacker_idx()),
               static_cast<int>(sv->defender_idx()),
               static_cast<int>(sv->phase()),
               static_cast<int>(sv->attacks_used()),
               static_cast<int>(sv->bout_cap()));

    // 1) Only act on my turn
    const bool my_turn =
        (sv->phase() == durak::gen::net::Phase::Attacking && sv->attacker_idx() == seat) ||
        (sv->phase() == durak::gen::net::Phase::Defending && sv->defender_idx() == seat);

    if (!my_turn)
    {
        std::print("[NetAI][seat {}] Not my turn — skipping.\n", static_cast<int>(seat));
        return;
    }

    // 2) Debounce: act at most once per distinct turn state
    static std::uint64_t last_sent_key[durak::core::constants::MaxPlayers] = {};
    const std::uint64_t turn_key = make_turn_key(sv);
    if (last_sent_key[seat] == turn_key)
    {
        std::print("[NetAI][seat {}] Already acted for this turn state — skipping.\n", static_cast<int>(seat));
        return;
    }

    // 3) Rebuild snapshot for AI
    SnapshotScratch scratch{};
    durak::core::GameSnapshot gs = to_snapshot(sv, scratch);

    // 4) Ask AI
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
    durak::core::PlayerAction act = ai.Play(std::make_shared<durak::core::GameSnapshot>(gs), deadline);

    // 5) Build outbound message — with legality filtering for Attack, and real Pass/Take support
    using durak::core::net::CardVal;
    using durak::core::net::DefPair;

    std::vector<std::uint8_t> out;

    bool sent = std::visit([&](auto const& a) -> bool
    {
        using T = std::decay_t<decltype(a)>;

        if constexpr (std::is_same_v<T, durak::core::AttackAction>)
        {
            // Legal-rank filter:
            //  - if table empty -> send exactly one card (first).
            //  - else -> only ranks already present on table.
            const auto mask = table_rank_mask(sv);
            const bool table_empty = (sv->attacks_used() == 0);

            std::vector<CardVal> vals;
            vals.reserve(a.cards.size());

            if (table_empty)
            {
                if (a.cards.empty()) { return false; }
                auto sp = a.cards.front().lock();
                if (!sp) { return false; }
                vals.push_back(CardVal{ sp->suit, sp->rank });
            }
            else
            {
                for (const durak::core::CardWP& w : a.cards)
                {
                    auto sp = w.lock();
                    if (!sp) { continue; }
                    int rk = static_cast<int>(sp->rank);
                    if (rk >= 0 && rk < static_cast<int>(mask.size()) && mask[rk])
                    {
                        vals.push_back(CardVal{ sp->suit, sp->rank });
                    }
                }
                if (vals.empty())
                {
                    std::print("[NetAI][seat {}] Attack filtered to 0 cards — not sending.\n", static_cast<int>(seat));
                    return false;
                }
            }

            out = durak::core::net::BuildAction_Attack(
                seat,
                std::span<const CardVal>(vals.data(), vals.size()),
                /*msg_id*/ 900 + seat);

            return true;
        }
        else if constexpr (std::is_same_v<T, durak::core::DefendAction>)
        {
            std::vector<DefPair> vals;
            vals.reserve(a.pairs.size());

            for (const durak::core::DefendPair& p : a.pairs)
            {
                auto atk = p.attack.lock();
                auto def = p.defend.lock();
                if (!atk || !def) { continue; }
                vals.push_back(DefPair{ CardVal{atk->suit, atk->rank},
                                        CardVal{def->suit, def->rank} });
            }
            if (vals.empty()) { return false; }

            out = durak::core::net::BuildAction_Defend(
                seat,
                std::span<const DefPair>(vals.data(), vals.size()),
                /*msg_id*/ 1000 + seat);

            return true;
        }
        else if constexpr (std::is_same_v<T, durak::core::PassAction>)
        {
            // BuildAction_Pass returns DetachedBuffer → copy to vector
            auto buf = durak::core::net::BuildAction_Pass(seat, /*msg_id*/ 1100 + seat);
            out.assign(buf.data(), buf.data() + buf.size());
            return true;
        }
        else if constexpr (std::is_same_v<T, durak::core::TakeAction>)
        {
            auto buf = durak::core::net::BuildAction_Take(seat, /*msg_id*/ 1200 + seat);
            out.assign(buf.data(), buf.data() + buf.size());
            return true;
        }
        else
        {
            return false;
        }
    }, act);

    if (!sent || out.empty())
    {
        std::print("[NetAI][seat {}] Failed to build outbound action.\n", static_cast<int>(seat));
        return;
    }

    try
    {
        c.send(*hdl_ptr, out.data(), out.size(), websocketpp::frame::opcode::binary);
        last_sent_key[seat] = turn_key; // mark success
        std::print("[NetAI][seat {}] Sent action ({} bytes).\n", static_cast<int>(seat), static_cast<int>(out.size()));
    }
    catch (const std::exception& e)
    {
        std::print("[NetAI] send() failed: {}\n", e.what());
    }
});


    c.set_open_handler([&](websocketpp::connection_hdl hdl)
    {
        *hdl_ptr = hdl;
        opened = true;
        std::print("[NetAI] Connected.\n");
    });

    c.set_close_handler([&](websocketpp::connection_hdl)
    {
        std::print("[NetAI] Closed by server.\n");
    });

    websocketpp::lib::error_code ec;
    WsClient::connection_ptr con = c.get_connection(cfg.url, ec);
    if (ec)
    {
        std::print("[NetAI] get_connection error: {}\n", ec.message());
        return 2;
    }

    c.connect(con);

    // Run the client loop (blocking)
    c.run();

    return 0;
}
