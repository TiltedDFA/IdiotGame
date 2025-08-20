#include "AuditLogger.hpp"

#include <array>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

using namespace durak::core;

namespace
{

auto s_suit(Suit const s) -> std::string_view
{
    switch (s)
    {
        case Suit::Clubs:    return "C";
        case Suit::Diamonds: return "D";
        case Suit::Hearts:   return "H";
        case Suit::Spades:   return "S";
    }
    return "?";
}

auto s_rank(Rank const r) -> std::string_view
{
    static constexpr std::array<std::string_view, 13> map{
        "2","3","4","5","6","7","8","9","T","J","Q","K","A"
    };
    return map[static_cast<size_t>(r)];
}

auto s_card(Card const& c) -> std::string
{
    return std::format("{}{}", s_rank(c.rank), s_suit(c.suit));
}

auto s_action(PlayerAction const& a) -> std::string
{
    return std::visit(
        [&]<typename T0>(T0 const& act) -> std::string
        {
            using T = std::decay_t<T0>;

            if constexpr (std::is_same_v<T, AttackAction>)
            {
                std::vector<std::string> parts;
                parts.reserve(act.cards.size());

                for (CardWP const& w : act.cards)
                {
                    auto const sp = w.lock();
                    if (sp)
                    {
                        parts.emplace_back(s_card(*sp));
                    }
                }

                std::string body;
                for (size_t i{}; i < parts.size(); ++i)
                {
                    body += (i ? "," : "");
                    body += parts[i];
                }
                return std::format("Attack[{}]", body);
            }
            else if constexpr (std::is_same_v<T, DefendAction>)
            {
                std::vector<std::string> parts;
                parts.reserve(act.pairs.size());

                for (DefendPair const& p : act.pairs)
                {
                    auto const a1 = p.attack.lock();
                    auto const d1 = p.defend.lock();
                    if (a1 && d1)
                    {
                        parts.emplace_back(std::format("{}/{}", s_card(*a1), s_card(*d1)));
                    }
                }

                std::string body;
                for (size_t i{}; i < parts.size(); ++i)
                {
                    body += (i ? "," : "");
                    body += parts[i];
                }
                return std::format("Defend{{{}}}", body);
            }
            else if constexpr (std::is_same_v<T, ThrowInAction>)
            {
                std::vector<std::string> parts;
                parts.reserve(act.cards.size());

                for (CardWP const& w : act.cards)
                {
                    auto const sp = w.lock();
                    if (sp)
                    {
                        parts.emplace_back(s_card(*sp));
                    }
                }

                std::string body;
                for (size_t i{}; i < parts.size(); ++i)
                {
                    body += (i ? "," : "");
                    body += parts[i];
                }
                return std::format("ThrowIn[{}]", body);
            }
            else if constexpr (std::is_same_v<T, TransferAction>)
            {
                if (auto const sp = act.card.lock())
                {
                    return std::format("Transfer({})", s_card(*sp));
                }
                return "Transfer(?)";
            }
            else if constexpr (std::is_same_v<T, PassAction>)
            {
                return "Pass";
            }
            else
            {
                return "Take";
            }
        },
        a
    );
}

auto serialize_table(GameSnapshot const& s) -> std::string
{
    std::string serial;
    bool first = true;

    for (TableSlotView const& slot : s.table)
    {
        auto const a1 = slot.attack.lock();
        auto const d1 = slot.defend.lock();

        if (!a1 && !d1)
        {
            continue;
        }

        serial += (first ? "" : ",");
        first = false;

        serial += std::format(
            "{}/{}",
            a1 ? s_card(*a1) : std::string("--"),
            d1 ? s_card(*d1) : std::string("--")
        );
    }

    return serial;
}

} // anonymous namespace

namespace durak::core::debug
{

AuditLogger::AuditLogger(std::string path)
    : out_(std::move(path), std::ios::out | std::ios::trunc)
{
}

AuditLogger::~AuditLogger() = default;

auto AuditLogger::start(GameImpl const& game, uint64_t seed) -> void
{
    out_ << std::format("Seed={}\n", seed);
    out_ << std::format("Trump={}\n", s_suit(game.Trump()));
    out_ << std::format("Players={}\n", static_cast<int>(game.PlayerCount()));
    out_.flush();
}

auto AuditLogger::turn(GameSnapshot const& s,
                       uint8_t actor,
                       PlayerAction const& a) -> void
{
    out_ << std::format(
        "Turn actor=P{} phase={} atk={} def={} table=[{}]\n",
        static_cast<int>(actor),
        (s.phase == Phase::Attacking ? "A" : "D"),
        static_cast<int>(s.attacker_idx),
        static_cast<int>(s.defender_idx),
        serialize_table(s)
    );

    out_ << std::format("Action: {}\n", s_action(a));
}

auto AuditLogger::turn(GameSnapshot const& s,
                       uint8_t actor) -> void
{
    out_ << std::format(
        "Turn actor=P{} phase={} atk={} def={} table=[{}]\n",
        static_cast<int>(actor),
        (s.phase == Phase::Attacking ? "A" : "D"),
        static_cast<int>(s.attacker_idx),
        static_cast<int>(s.defender_idx),
        serialize_table(s)
    );

    out_ << "Action: <omitted>\n";
}

auto AuditLogger::outcome(MoveOutcome m) -> void
{
    char const* txt =
        (m == MoveOutcome::Applied    ? "Applied" :
        (m == MoveOutcome::RoundEnded ? "RoundEnded" :
        (m == MoveOutcome::GameEnded  ? "GameEnded" : "Invalid")));
    out_ << std::format("Outcome: {}\n", txt);
}

auto AuditLogger::cleanup(GameImpl const& game) -> void
{
    std::string body;

    for (std::uint8_t i = 0; i < game.PlayerCount(); ++i)
    {
        auto const snap = game.SnapshotFor(i);
        body += std::format(
            "{}{}:{}",
            (i ? "," : ""),
            static_cast<int>(i),
            snap->my_hand.size()
        );
    }

    out_ << std::format("Cleanup: handsizes=[{}]\n", body);
}

auto AuditLogger::end(GameImpl const& game) -> void
{
    int loser = -1;

    for (uint8_t i = 0; i < game.PlayerCount(); ++i)
    {
        auto const snap = game.SnapshotFor(i);
        loser = (loser == -1 && !snap->my_hand.empty())
              ? static_cast<int>(i) : loser;
    }

    out_ << std::format("Loser={}\n", loser);
    out_.flush();
}

auto AuditLogger::flush() -> void
{
    out_.flush();
}

} // namespace durak::audit