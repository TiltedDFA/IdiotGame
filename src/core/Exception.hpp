//
// Created by Malik T on 14/08/2025.
//

#ifndef IDIOTGAME_EXCEPTION_HPP
#define IDIOTGAME_EXCEPTION_HPP

#include "OmegaException.hpp"

#include <expected>
#include <format>
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
        Unknown,        // unknown error
        Rules,          // rules engine misuse (not user invalid move)
        State,          // state engine misuse (not user invalid move)
        InvalidAction,  // user/remote proposed action cannot be applied
        Timeout,        // deadline exceeded for IO or player move
        Network,        // transport failure
        Serialization,  // FlatBuffers verification/build errors
        Assertion       // internal assertion failed
    };
    struct UnknownError : public OmegaException<Code>       { using OmegaException<Code>::OmegaException; };
    struct RulesError : public OmegaException<Code>         { using OmegaException<Code>::OmegaException; };
    struct StateError : public OmegaException<Code>         { using OmegaException<Code>::OmegaException; };
    struct InvalidActionError : public OmegaException<Code> { using OmegaException<Code>::OmegaException; };
    struct TimeoutError : public OmegaException<Code>       { using OmegaException<Code>::OmegaException; };
    struct NetworkError : public OmegaException<Code>       { using OmegaException<Code>::OmegaException; };
    struct SerializationError : public OmegaException<Code> { using OmegaException<Code>::OmegaException; };
    struct AssertionError : public OmegaException<Code>     { using OmegaException<Code>::OmegaException; };

    [[noreturn]]
    inline auto fail (Code c, std::string msg) -> void
    {
        switch (c)
        {
            case Code::Unknown:         throw UnknownError(std::move(msg), c);
            case Code::Rules:           throw RulesError(std::move(msg), c);
            case Code::State:           throw StateError(std::move(msg), c);
            case Code::InvalidAction:   throw InvalidActionError(std::move(msg), c);
            case Code::Timeout:         throw TimeoutError(std::move(msg), c);
            case Code::Network:         throw NetworkError(std::move(msg), c);
            case Code::Serialization:   throw SerializationError(std::move(msg), c);
            case Code::Assertion:       throw AssertionError(std::move(msg), c);
        }
    }
    //leaving it simple for now, can add more specific issues later
    enum class RuleViolation : uint8_t { All };

    using ValidateResult = std::expected<void, RuleViolation>;

    #define DRK_THROW(code_enum, msg) ::durak::core::error::fail((code_enum), (msg))
    #define DRK_ASSERT(cond, msg) do { if(!(cond)) ::durak::core::error::fail(::durak::core::error::Code::Assertion, (msg)); } while(0)
}

#endif //IDIOTGAME_EXCEPTION_HPP