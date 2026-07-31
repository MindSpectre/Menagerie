#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>

#include "detail/log_event.hpp"
#include "detail/sink_status.hpp"

namespace menagerie::crow {
    class Logger;

    /**
     * @brief Base interface for all log sinks.
     *
     * Non-templated to allow heterogeneous storage in Logger.
     * Each sink implementation chooses its EntryType for formatting.
     *
     * Design:
     * - Logger stores vector<shared_ptr<Sink>>
     * - ConsoleSink<DetailedEntry>, FileSink<LightEntry> both inherit from Sink
     * - Consumer dispatches batches to sink strands via process_batch()
     * - Sink converts LogEvent -> EntryType -> formatted output
     */
    class Sink {
    public:
        virtual ~Sink() = default;

        /**
         * @brief Processes a single log event.
         *
         * Never throws: a sink that cannot write records the failure with set_status()
         * and returns. The Logger skips Dead sinks and the janitor retries them.
         */
        virtual void process(const LogEvent& event) noexcept = 0;

        /**
         * @brief Processes a batch of log events.
         *
         * Default: iterates and calls process() per event.
         * Override for batch-optimized I/O (e.g., batch network sends).
         */
        virtual void process_batch(const std::shared_ptr<std::vector<LogEvent>>& batch) noexcept {
            for (const auto& event : *batch) {
                process(event);
            }
        }

        /**
         * @brief Flushes any buffered data.
         *
         * Called during:
         * - Logger shutdown
         * - Explicit flush requests
         * - Critical errors (ERR/FAT logs)
         */
        virtual void flush() noexcept = 0;

        /**
         * @brief Checks if this sink should process this log level, for early
         *        filtering before calling process().
         * @return true if the sink will process this level/prefix combination.
         */
        [[nodiscard]] virtual bool should_log(LogLevel lvl, std::string_view prefix) const noexcept = 0;

        /// Recovers from failure, rotates, or does nothing. Runs on the sink's strand,
        /// so it is serialized against process_batch(). Default: no-op.
        virtual void maintain() noexcept {
        }

        /// One decoded read of the packed dispatch word: status, threshold, retry deadline.
        [[nodiscard]] DispatchHint dispatch_hint() const noexcept {
            return detail::unpack_dispatch(dispatch_word_.load(std::memory_order_relaxed));
        }

        /// The sink's current lifecycle state.
        [[nodiscard]] SinkStatus get_status() const noexcept {
            return dispatch_hint().status;
        }

        /// Events the Logger did not deliver because this sink was Dead. Upper bound:
        /// counted per batch, not filtered by this sink's prefix filter -- but a batch
        /// this sink's threshold could never have accepted (checked against the
        /// batch's highest level) is skipped by the dispatcher and never added here.
        [[nodiscard]] std::uint64_t undelivered() const noexcept {
            return undelivered_.load(std::memory_order_relaxed);
        }

        /// Adds to the undelivered count. Called by the Logger when it skips this sink.
        void add_undelivered(const std::uint64_t count) noexcept {
            undelivered_.fetch_add(count, std::memory_order_relaxed);
        }

        /// Reason recorded with the most recent status change; empty if never failed.
        /// Pairs with the status published at or before the status a concurrent reader
        /// observes from dispatch_hint()/get_status() — never with a later one, since
        /// set_status() writes the reason before it commits the status (see below).
        [[nodiscard]] std::string last_error() const {
            std::lock_guard lock{state_mutex_};
            return last_error_;
        }

    protected:
        // set_status(), publish_threshold(), note_failure(), and note_success() below
        // are the sink's protected extension point. They are not mutually safe under
        // concurrent calls to each other: a sink implementation must serialize them
        // (e.g. by only calling them from its own asio strand, as the finished design
        // does). This is not enforced with a lock — that would put a mutex on a path
        // the design keeps lock-free — so concurrent calls can lose updates (see
        // note_failure()'s comment on the backoff count).

        /// Records a lifecycle change and the reason for it, preserving threshold and
        /// retry deadline. An empty reason clears the stored error. Writes the reason
        /// before committing the status, so a reader that observes the new status via
        /// dispatch_hint()/get_status() and then calls last_error() always sees the
        /// reason that accompanied it, never a stale one from before this call.
        void set_status(const SinkStatus status, const std::string_view reason) noexcept {
            {
                std::lock_guard lock{state_mutex_};
                last_error_.assign(reason);
            }
            update_dispatch([status](const DispatchHint& hint) {
                return detail::pack_dispatch(status, hint.threshold, hint.retry_at_ms);
            });
        }

        /// Publishes the threshold the Logger's gate aggregates. Call from set_config().
        void publish_threshold(const LogLevel threshold) noexcept {
            update_dispatch([threshold](const DispatchHint& hint) {
                return detail::pack_dispatch(hint.status, threshold, hint.retry_at_ms);
            });
        }

        /// Marks a failed maintenance attempt: bumps the failure count and pushes the
        /// retry deadline out by the backoff for that count. The failure-count bump and
        /// the dispatch-word write are two separate atomic operations; concurrent calls
        /// can interleave so the published retry_at_ms is computed from a stale, lower
        /// failure count, silently under-backing-off. Callers must serialize calls to
        /// this method with respect to each other (e.g. via the sink's strand).
        void note_failure() noexcept {
            const auto failures = consecutive_failures_.fetch_add(1, std::memory_order_relaxed);
            const auto retry_at = detail::steady_now_ms() + detail::backoff_ms(failures);
            update_dispatch([retry_at](const DispatchHint& hint) {
                return detail::pack_dispatch(hint.status, hint.threshold, retry_at);
            });
        }

        /// Clears the failure count and retry deadline after a successful recovery.
        /// Same serialization requirement as note_failure(): callers must not invoke
        /// this concurrently with another note_failure()/note_success() call.
        void note_success() noexcept {
            consecutive_failures_.store(0, std::memory_order_relaxed);
            update_dispatch(
                [](const DispatchHint& hint) { return detail::pack_dispatch(hint.status, hint.threshold, 0); });
        }

    private:
        /// Applies `transform` to a decoded read of the dispatch word and retries the CAS
        /// until it wins, so concurrent updates (e.g. a threshold publish racing a status
        /// change) always compose instead of clobbering one another.
        template <typename Fn>
        void update_dispatch(Fn&& transform) noexcept {
            std::uint64_t expected = dispatch_word_.load(std::memory_order_relaxed);
            while (!dispatch_word_.compare_exchange_weak(
                expected, transform(detail::unpack_dispatch(expected)), std::memory_order_relaxed)) {}
        }

        std::atomic<std::uint64_t> dispatch_word_{detail::pack_dispatch(SinkStatus::Healthy, LogLevel::Trace, 0)};
        std::atomic<std::uint64_t> undelivered_{0};
        std::atomic<std::uint32_t> consecutive_failures_{0};
        mutable std::mutex state_mutex_;
        std::string last_error_;

        /// The last status the registering Logger's janitor reported for this sink,
        /// written only under that Logger's sweep_mutex_. Lives on the sink rather than
        /// in a Logger-side map keyed by Sink* so a freed sink's address being reused by
        /// a new allocation can never resurrect stale state. Starting at Healthy makes
        /// first sight of a healthy sink a non-transition, while a sink that arrives
        /// broken reports Healthy -> Dead on its first sweep.
        friend class Logger;
        SinkStatus reported_status_ = SinkStatus::Healthy;
    };

    /// Per-sink slot: pairs a sink with its asio::strand for serial dispatch.
    struct SinkSlot {
        std::shared_ptr<Sink> sink;                                ///< The registered sink.
        boost::asio::strand<boost::asio::any_io_executor> strand;  ///< Strand serializing dispatch to sink.
    };

    /// One sink's health, as reported by Logger::sink_report().
    struct SinkReport {
        std::shared_ptr<Sink> sink;  ///< The registered sink.
        SinkStatus status;           ///< Its lifecycle state at report time.
        std::uint64_t undelivered;   ///< Events not delivered while it was Dead.
        std::string last_error;      ///< Reason recorded with the most recent failure.
    };

    /// One sink lifecycle transition, as observed by the janitor.
    struct SinkFailure {
        std::shared_ptr<const Sink> sink;  ///< The sink that changed state; safe to retain past the callback.
        SinkStatus from;                   ///< Status at the previous sweep.
        SinkStatus to;                     ///< Status now.
        std::string_view reason;           ///< The sink's last recorded error; empty on recovery. Borrowed from the
                                           ///< pending report -- valid only for the callback's duration, unlike sink.
        std::uint64_t undelivered;         ///< Events not delivered while it was Dead.
    };

}  // namespace menagerie::crow
