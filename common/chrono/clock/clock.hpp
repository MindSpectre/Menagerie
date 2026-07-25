#pragma once
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

/// Menagerie's timing toolkit: wall-clock formatting/parsing, HTTP-date rendering, a
/// raw hardware tick counter, stopwatches, and a deadline-bound function executor.
namespace menagerie::chrono {
    namespace detail {
        /// Satisfied by string-like types with an `append(const char*, size_t)` method
        /// (e.g. std::string), used to append-in-place instead of allocating a new string.
        template <typename T>
        concept DoesStringHaveAppendMethod = requires(T& s, const char* ptr, std::size_t n) {
            { s.append(ptr, n) };
        };
    }  // namespace detail

    // -------- Base clock (parse & now) --------
    /// Base clock: wraps std::chrono::system_clock with a parse() helper. SpecClock
    /// (LocalClock/UTCClock) adds locale-aware formatting on top.
    class Clock {
    public:
        using sys_tp = std::chrono::time_point<std::chrono::system_clock>;  ///< System-clock time point alias.

        /// The current system-clock time point.
        [[nodiscard]] static sys_tp now() noexcept {
            return std::chrono::system_clock::now();
        }

        /// Parses txt as a strftime-style fmt string into a system_clock time point.
        /// @throw std::invalid_argument if txt does not match fmt.
        template <class Dur = std::chrono::milliseconds>
        [[nodiscard]] static std::chrono::time_point<std::chrono::system_clock, Dur> parse(const std::string_view txt,
                                                                                           std::string_view fmt) {
            using namespace std::chrono;
            std::istringstream is(std::string{txt});
            sys_time<Dur> tp;
            is >> parse(fmt.data(), tp);
            if (is.fail()) {
                throw std::invalid_argument("Invalid time format " + std::string{fmt} +
                                            " or value:" + std::string{txt});
            }
            return tp;
        }
    };

    // -------- Canonical patterns --------
    /// Canonical strftime pattern strings for SpecClock::current_time() / format_time().
    namespace clock_formats {
        constexpr auto eu_dmy_hms = "%d-%m-%Y %H:%M:%S";  ///< "31-12-2025 23:59:00"
        constexpr auto us_mdy_hms = "%m-%d-%Y %I:%M:%S %p";  ///< "12-31-2025 11:59:00 PM"
        constexpr auto iso8601    = "%Y-%m-%dT%H:%M:%S";  ///< "2025-12-31T23:59:00"
    }  // namespace clock_formats

    // -------- Local / UTC clocks --------
    /// Which locale SpecClock formats/parses with.
    enum class ClockType {
        Local,  ///< Uses localtime_r; format_time_iso_ms() omits the trailing "Z".
        UTC     ///< Uses gmtime_r; format_time_iso_ms() appends a trailing "Z".
    };

    /// Locale-specific clock (LocalClock or UTCClock): formats/parses system_clock
    /// time points using either localtime_r or gmtime_r, selected by CT.
    template <ClockType CT>
    class SpecClock : public Clock {
    public:
        // -------- PUBLIC API --------
        /// now(), formatted with fmt.
        [[nodiscard]] static std::string current_time(const std::string_view fmt) {
            return format_time(now(), fmt);
        }

        /// Renders tp as millisecond-precision ISO 8601 ("...T...Z" for UTC, no "Z" for Local).
        static std::string format_time_iso_ms(const sys_tp tp) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            const auto tm = to_tm(tp);
            char timestamp[32];
            std::int32_t len;
            if constexpr (CT == ClockType::UTC) {
                len = snprintf(timestamp,
                               sizeof(timestamp),
                               "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                               tm.tm_year + 1900,
                               tm.tm_mon + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               ms.count());
            } else {
                len = snprintf(timestamp,
                               sizeof(timestamp),
                               "%04d-%02d-%02dT%02d:%02d:%02d.%03lld ",
                               tm.tm_year + 1900,
                               tm.tm_mon + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               ms.count());
            }

            return {timestamp, static_cast<std::size_t>(len)};
        }

        /// Renders tp the same way as the returning overload, appending into out instead
        /// of allocating a new string.
        template <detail::DoesStringHaveAppendMethod StringT>
        static void format_time_iso_ms(const sys_tp tp, StringT& out) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            const auto tm = to_tm(tp);
            char timestamp[32];
            std::int32_t len;
            if constexpr (CT == ClockType::UTC) {
                len = snprintf(timestamp,
                               sizeof(timestamp),
                               "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                               tm.tm_year + 1900,
                               tm.tm_mon + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               ms.count());
            } else {
                len = snprintf(timestamp,
                               sizeof(timestamp),
                               "%04d-%02d-%02dT%02d:%02d:%02d.%03lld ",
                               tm.tm_year + 1900,
                               tm.tm_mon + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               ms.count());
            }

            out.append(timestamp, static_cast<std::size_t>(len));
        }
        /// Renders tp with a strftime-style format string (see clock_formats for
        /// canonical patterns).
        [[nodiscard]] static std::string format_time(const sys_tp tp, const std::string_view format) {
            std::ostringstream ss;
            const std::tm buf = to_tm(tp);
            ss << std::put_time(&buf, format.data());
            return ss.str();
        }

        /// Converts tp to a broken-down std::tm via localtime_r (Local) or gmtime_r (UTC).
        [[nodiscard]] static std::tm to_tm(const sys_tp tp) {
            const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
            std::tm out{};
            if constexpr (CT == ClockType::Local) {
                localtime_r(&tt, &out);
            } else {
                gmtime_r(&tt, &out);
            }
            return out;
        }
    };

    using LocalClock = SpecClock<ClockType::Local>;  ///< Formats/parses using the process's local timezone.
    using UTCClock   = SpecClock<ClockType::UTC>;    ///< Formats/parses using UTC.
}  // namespace menagerie::chrono
