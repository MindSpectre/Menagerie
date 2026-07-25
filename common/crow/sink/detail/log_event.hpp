#pragma once

#include <string>

#include <entry_interface.hpp>

namespace menagerie::crow {
    /**
     * @brief Raw log event data container
     *
     * Contains ALL possible metadata captured in producer thread.
     * Sinks extract what their EntryType needs via make_entry_from_event().
     *
     * Stored in RingBuffer for lock-free producer-consumer pattern.
     */
    struct LogEvent {
        // Core data
        LogLevel level = LogLevel::Debug;    ///< Severity level.
        PrefixNameStorage prefix{};          ///< Owning class-name/prefix; empty if none.
        std::string message;                 ///< Already formatted with std::format or stream.

        // Metadata (captured in producer thread - correct TID/PID)
        detail::MetaSource location{};    ///< Call site the event was logged from.
        detail::MetaTimePoint time_point{};  ///< Wall-clock time the event was constructed.
        detail::MetaThread tid{};            ///< Producer thread id.
        detail::MetaProcess pid{};           ///< Process id.

        bool shutdown_signal = false;  ///< Sentinel event telling the consumer thread to stop.

        LogEvent() = default;
    };

    /**
     * @brief Builds an EntryT (DetailedEntry, LightEntry, ...) from a LogEvent instead
     *        of capturing fresh metadata, so TID/PID reflect the producer thread that
     *        logged the event, not the consumer thread draining it.
     *
     * Usage:
     *   auto entry = make_entry_from_event<DetailedEntry>(event);
     *   entry.format_into(buffer);
     */
    template <class EntryT>
    EntryT make_entry_from_event(const LogEvent& event) {
        // Build tuple with all available metadata from LogEvent
        auto available =
            std::tuple{event.time_point, event.location, event.tid, event.pid, detail::MetaPrefix{event.prefix.view()}};

        // Use entry_traits to extract only what EntryT wants
        using want_types = detail::entry_traits<EntryT>::wants;
        auto args        = beavers::make_arg_tuple<want_types, decltype(available)>::from(std::move(available));

        // Construct entry with level, message, and tailored metadata
        return std::apply(
            [&]<typename... Tailored>(Tailored&&... tail) {
                return EntryT{event.level, event.message, std::forward<Tailored>(tail)...};
            },
            args);
    }
}  // namespace menagerie::crow
