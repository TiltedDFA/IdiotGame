//
// Created by Malik T on 26/09/2025.
//
#include "Judge.hpp"
#include <algorithm>
#include <future>
#include <thread>
#include <utility>
#include "Exception.hpp"
#include "Game.hpp"
#include "Player.hpp"

namespace durak::core
{
    static auto TableHasAnyAttack(GameSnapshot const& s) -> bool
    {
        return std::ranges::any_of(s.table, [](TableSlotView const& ts)
            {return !ts.attack.expired();});
    }

    static auto MakeDefaultAttack(GameSnapshot const& s) -> PlayerAction
    {
        if (s.my_hand.empty())
            return PassAction{};

        //looks for smallest card
        CardWP best{};
        CCardSP best_sp{};

        for (auto const& c : s.my_hand)
        {
            auto sp = c.lock();
            if (!sp) continue;

            if (!best_sp)
            {
                best_sp = sp;
                best = c;
                continue;
            }
            auto lhs = std::to_underlying(sp->rank);
            auto rhs = std::to_underlying(best_sp->rank);
            bool const better = (lhs < rhs) || (lhs == rhs && std::to_underlying(sp->suit) < std::to_underlying(best_sp->suit));
            if (better)
            {
                best = c;
                best_sp = sp;
            }
        }
        if (!best_sp) [[unlikely]]
        {DRK_THROW(durak::core::error::Code::State, "Best did not exist at return time");};
        return AttackAction{ std::vector<CardWP>{best}};
    }

    auto Judge::GetAction(GameImpl& game, PlyrIdxT actor) const -> TimedDecision
    {
        std::shared_ptr<const GameSnapshot> snap = game.SnapshotFor(actor);
        auto const deadline = std::chrono::steady_clock::now() + game.cfg_.turn_timeout;

        std::packaged_task<PlayerAction()> task(
            [p = game.PlayerAt(actor),
             snp = std::move(snap),
             deadline]() mutable
            {
                return p->Play(std::move(snp), deadline);
            }
        );

        std::future<PlayerAction> fut = task.get_future();


        std::thread worker(std::move(task));
        worker.detach();

        if (fut.wait_until(deadline) == std::future_status::ready)
        {
            return {fut.get(), DesicionResult::OK};
        }

        //Timeout
        TimedDecision res{};
        res.result = DesicionResult::Timeout;
        std::shared_ptr<const GameSnapshot> s_now = game.SnapshotFor(actor);
        if (s_now->phase == Phase::Defending)
        {
            res.action = TakeAction{};
            return res;
        }

        bool const table_has_attacks = TableHasAnyAttack(*s_now);
        res.action = table_has_attacks ? PlayerAction{PassAction{}} : MakeDefaultAttack(*s_now);
        return res;
    }

}