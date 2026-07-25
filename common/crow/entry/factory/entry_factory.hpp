#pragma once

namespace menagerie::crow {
    /// Builds an EntryT, selecting only the Meta* mixins entry_traits<EntryT> declares
    /// (via beavers::make_arg_tuple) out of the full set this function constructs.
    template <class EntryT, class... Extra>
    EntryT make_entry(LogLevel lvl, const std::string_view msg, const std::source_location& loc, Extra&&... extra) {
        // Use cached thread-local values
        auto available = std::tuple{detail::MetaTimePoint{},
                                    detail::MetaSource{loc},
                                    // Already lightweight
                                    detail::MetaThread{},
                                    // Uses cached values
                                    detail::MetaProcess{},
                                    // Uses cached values
                                    detail::MetaPrefix{},
                                    // Empty by default; prefix is set via make_entry_from_event
                                    std::forward<Extra>(extra)...};

        using want_types = detail::entry_traits<EntryT>::wants;
        auto args        = beavers::make_arg_tuple<want_types, decltype(available)>::from(std::move(available));

        return std::apply(
            [&]<typename... Tailored>(Tailored&&... tail) { return EntryT{lvl, msg, std::forward<Tailored>(tail)...}; },
            args);
    }
}  // namespace menagerie::crow
