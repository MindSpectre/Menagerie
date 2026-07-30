#pragma once

#include <chrono>
#include <cstdint>
#include <menagerie/chrono>

// Resolves through the entry/ include directory that ${CROW}.Sink inherits from
// ${CROW}.Entry — the same form entry_interface.hpp uses.
#include "detail/log_level.hpp"

namespace menagerie::crow {

    /// Lifecycle state of a sink, as seen by the Logger's dispatch and janitor.
    enum class SinkStatus : std::uint8_t {
        Healthy,   ///< Accepting and writing.
        Degraded,  ///< Still writing, but a maintenance action is failing (e.g. rotation).
        Dead,      ///< Cannot write; events are not delivered until recovery succeeds.
    };

    /// Maps a SinkStatus to its debug name, or "UNKNOWN" if the value is out of range.
    [[nodiscard]] constexpr const char* to_string(const SinkStatus status) noexcept {
        switch (status) {
            case SinkStatus::Healthy:
                return "HEALTHY";
            case SinkStatus::Degraded:
                return "DEGRADED";
            case SinkStatus::Dead:
                return "DEAD";
        }
        return "UNKNOWN";
    }

    /// One decoded read of a sink's packed dispatch word.
    struct DispatchHint {
        SinkStatus status;          ///< Current lifecycle state.
        LogLevel threshold;         ///< Minimum level the sink accepts.
        std::uint64_t retry_at_ms;  ///< steady_clock ms before which maintenance must not retry.
    };

    namespace detail {
        /// Threshold no level clears; the Logger's gate holds it while no sinks are registered.
        inline constexpr std::uint8_t drop_all_threshold = 0x7F;

        /// Recovery retry schedule, fed to menagerie::chrono::exponential_backoff.
        inline constexpr std::chrono::milliseconds backoff_base{1000};
        inline constexpr std::chrono::milliseconds backoff_cap{60000};

        // Dispatch word layout: [63..16] retry_at_ms | [15..8] SinkStatus | [7..0] LogLevel.
        // One relaxed load gives the consumer everything it needs to decide whether to post.

        [[nodiscard]] constexpr std::uint64_t
        pack_dispatch(const SinkStatus status, const LogLevel threshold, const std::uint64_t retry_at_ms) noexcept {
            return retry_at_ms << 16 | static_cast<std::uint64_t>(status) << 8 | static_cast<std::uint64_t>(threshold);
        }

        [[nodiscard]] constexpr DispatchHint unpack_dispatch(const std::uint64_t word) noexcept {
            return DispatchHint{.status      = static_cast<SinkStatus>(word >> 8 & 0xFF),
                                .threshold   = static_cast<LogLevel>(word & 0xFF),
                                .retry_at_ms = word >> 16};
        }

        /// Milliseconds on the steady clock: the time base for retry deadlines.
        [[nodiscard]] inline std::uint64_t steady_now_ms() noexcept {
            return menagerie::chrono::steady_since_epoch<std::chrono::milliseconds>();
        }

        /// Delay before the next recovery attempt: the first retry is immediate, then 1s
        /// doubling per consecutive failure from there (1s, 2s, 4s, ..., 32s), capped at 60s.
        ///
        /// The doubling itself is chrono::exponential_backoff, whose attempt parameter is
        /// 0-based -- hence the -1, which shifts the schedule so that the *second* failure
        /// is the one that waits a base interval.
        [[nodiscard]] constexpr std::uint64_t backoff_ms(const std::uint32_t consecutive_failures) noexcept {
            if (consecutive_failures == 0) {
                return 0;  // first retry may happen immediately; backoff starts after it fails
            }
            return static_cast<std::uint64_t>(
                menagerie::chrono::exponential_backoff(consecutive_failures - 1, backoff_base, backoff_cap)
                    .count());
        }
    }  // namespace detail

    /// True once a failed sink's backoff deadline has passed.
    [[nodiscard]] constexpr bool retry_due(const DispatchHint& hint, const std::uint64_t now_ms) noexcept {
        return now_ms >= hint.retry_at_ms;
    }
}  // namespace menagerie::crow
