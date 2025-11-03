//
// Created by Malik T on 13/08/2025.
//

#ifndef IDIOTGAME_OMEGAEXCEPTION_HPP
#define IDIOTGAME_OMEGAEXCEPTION_HPP
#include <source_location>
#include <stacktrace>
#include "Types.hpp"

namespace durak::core
{
    //inspired by CPPCon2023 "Exceptionally bad" by Peter Muldoon
    template <typename T>
    class OmegaException
    {
    public:
        OmegaException(std::string err_str,
                       T usr_data,
                       std::source_location const& src_loc = std::source_location::current(),
                       std::stacktrace backtrace = std::stacktrace::current()) :
            err_str_{std::move(err_str)},
            usr_data_{std::move(usr_data)},
            src_loc_{src_loc},
            backtrace_{backtrace}
        {
        }

        [[nodiscard]]
        auto what() -> std::string& { return err_str_; }

        [[nodiscard]]
        auto what() const noexcept -> std::string const& { return err_str_; }

        [[nodiscard]]
        auto where() const noexcept -> std::source_location const& { return src_loc_; }

        [[nodiscard]]
        auto stack() const noexcept -> std::stacktrace const& { return backtrace_; }

        auto data() -> T& { return usr_data_; }
        auto data() const noexcept -> T const& { return usr_data_; }

        [[nodiscard]]
        auto to_str() const -> std::string
        {
            std::string s = std::format("{}({}:{}), function `{}`\n", src_loc_.file_name(), src_loc_.line(),
                                        src_loc_.column(), src_loc_.function_name());
            for (auto it = backtrace_.begin(); it != (backtrace_.end() - 3); ++it)
            {
                s += std::format("{}({}):{}\n", it->source_file(), it->source_line(), it->description());
            }
            return s;
        }

    private:
        std::string err_str_;
        T usr_data_;
        std::source_location const src_loc_;
        std::stacktrace backtrace_;
    };
}

//extension to std format to allow use with std::print();
template <class T>
struct std::formatter<durak::core::OmegaException<T>> : std::formatter<std::string_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return std::formatter<std::string_view>::parse(ctx);
    }

    template <class FormatContext>
    auto format(durak::core::OmegaException<T> const& p, FormatContext& ctx) const
    {
        std::string s = std::format("Failed to process with code ({}): {}\n{}\n", static_cast<int>(p.data()), p.what(),
                                    p.to_str());
        return std::formatter<std::string_view>::format(s, ctx);
    }
};
#endif //IDIOTGAME_OMEGAEXCEPTION_HPP