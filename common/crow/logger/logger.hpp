#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <format>
#include <functional>
#include <memory>
#include <menagerie/multithread>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include "config/logger_config.hpp"
#include "sink_interface.hpp"

/// Menagerie's asynchronous logging stack: a Disruptor-backed Logger that buffers
/// events from any number of producer threads into a lock-free ring buffer and
/// dispatches them to sinks from one consumer thread, plus the entry types, sinks,
/// and LOG_*/COMPONENT_LOG_* macro API built around it.
namespace menagerie::crow {
    /**
     * @brief High-performance asynchronous logger using the Disruptor pattern.
     *
     * Features:
     * - Lock-free multi-producer logging via Disruptor ring buffer
     * - Single consumer thread batches and dispatches to sinks
     * - Non-templated (stores heterogeneous sinks via base class)
     * - Support for both format strings and stream-based logging
     * - Graceful shutdown ensures all events are processed
     * - add_sink(...)/remove_sink(...) register and unregister sinks, and
     *   sink_report() surfaces each registered sink's current health
     *
     * Architecture:
     *   Producer threads -> RingBuffer<LogEvent, 8192> -> Consumer thread -> Sinks
     *
     * Performance:
     * - ~10M events/sec throughput
     * - Sub-microsecond latency (P99 < 1us)
     * - Events no sink accepts are dropped before formatting
     *
     * Usage:
     *   Logger logger;
     *   logger.add_sink(std::make_unique<ConsoleSink<DetailedEntry>>(...));
     *   logger.add_sink(std::make_unique<FileSink<LightEntry>>(...));
     *
     *   // Format string style
     *   logger.log(LogLevel::Info, "User {} logged in", username);
     *
     *   // Stream style
     *   logger.stream(LogLevel::Info) << "User " << username << " logged in";
     */
    class Logger {
    public:
        /**
         * @brief Construct with external executor (recommended)
         *
         * The executor's thread pool runs sink processing. Multiple loggers
         * can share the same executor (e.g., with HTTP/DB components).
         */
        explicit Logger(boost::asio::any_io_executor executor,
                        const LoggerConfig& cfg = LoggerConfig::Builder{}.finalize())
            : disruptor_{cfg.ring_buffer_size(), create_wait_strategy(cfg.wait_strategy())},
              executor_{std::move(executor)} {
            start_threads(cfg);
        }

        /**
         * @brief Construct with internal thread pool (backward-compatible)
         *
         * Creates an internal asio::thread_pool sized by LoggerConfig::pool_size.
         */
        explicit Logger(const LoggerConfig& cfg = LoggerConfig::Builder{}.finalize())
            : disruptor_{cfg.ring_buffer_size(), create_wait_strategy(cfg.wait_strategy())},
              owned_pool_{std::in_place, cfg.pool_size()},
              executor_{owned_pool_->get_executor()} {
            start_threads(cfg);
        }

        ~Logger() {
            shutdown();
        }

        /**
         * @brief Registers a sink (ConsoleSink, FileSink, or a custom Sink) to receive
         *        log events.
         *
         * Each sink is paired with a strand on the executor for serial dispatch.
         * Thread-safe: may be called while other threads are logging.
         *
         * Registering the same sink object twice creates two independent strands for it,
         * so it then receives concurrent process_batch() calls from both -- breaking the
         * per-sink serialization every Sink implementation is entitled to assume. Register
         * each sink instance once.
         */
        void add_sink(std::shared_ptr<Sink> sink);

        /// Unregisters a sink by identity. Batches already posted to its strand still run.
        /// Removes only the first matching slot: if the same sink was registered more than
        /// once (see add_sink()'s caveat), call this once per registration.
        /// @return false if the sink was not registered.
        bool remove_sink(const std::shared_ptr<Sink>& sink);

        /// Health snapshot of every registered sink.
        [[nodiscard]] std::vector<SinkReport> sink_report() const;

        /// Aggregate minimum threshold across registered sinks; producers drop below it
        /// before formatting. detail::drop_all_threshold means no sink accepts anything.
        [[nodiscard, gnu::always_inline]] std::uint8_t gate_threshold() const noexcept {
            return gate_.load(std::memory_order_relaxed);
        }

        /// Logs with a std::format format string. prefix is a logger/class
        /// prefix; pass an empty string_view if none.
        template <typename... Args>
        void log(const LogLevel lvl,
                 const std::string_view prefix,
                 const std::source_location& loc,
                 std::format_string<Args...> fmt,
                 Args&&... args) {
            if (!passes_gate(lvl)) {
                return;  // no sink would accept this: skip formatting and publishing entirely
            }
            // COROUTINE SAFETY: No suspension points allowed between tl_msg_buf usage and publish_event().
            thread_local std::string tl_msg_buf;
            tl_msg_buf.clear();
            std::format_to(std::back_inserter(tl_msg_buf), fmt, std::forward<Args>(args)...);
            publish_event(lvl, prefix, tl_msg_buf, loc);
        }

        /**
         * @brief Log with a simple message + prefix
         */
        void log(const LogLevel lvl,
                 const std::string_view prefix,
                 const std::string_view msg,
                 const std::source_location& loc = std::source_location::current()) {
            if (!passes_gate(lvl)) {
                return;  // no sink would accept this: skip formatting and publishing entirely
            }
            // publish_event() takes its message by mutable reference and swaps it into the
            // ring slot (zero-copy transfer); msg is a borrowed view, so it must be staged
            // into an owned buffer first. This costs exactly the one copy this overload
            // always cost, and keeps the claim/publish tail itself down to one copy site.
            thread_local std::string tl_msg_buf;
            tl_msg_buf.assign(msg);
            publish_event(lvl, prefix, tl_msg_buf, loc);
        }

        /**
         * @brief Shorthand: log with simple message, no prefix.
         *
         * Kept permanently for callers that don't care about prefix (tests,
         * utility code). Forwards to the prefix-aware overload with empty prefix.
         */
        void log(const LogLevel lvl,
                 const std::string_view msg,
                 const std::source_location& loc = std::source_location::current()) {
            log(lvl, std::string_view{}, msg, loc);
        }

        /**
         * @brief Stream-based logging proxy carrying level + prefix + source location
         */
        class StreamProxy {
        public:
            /// Binds this proxy to logger, capturing level, prefix, and call site; the
            /// accumulated stream output is published as one event on destruction.
            template <typename SourceLocationTp = std::source_location>
                requires std::is_same_v<std::remove_cvref_t<SourceLocationTp>, std::source_location>
            StreamProxy(Logger* logger,
                        const LogLevel lvl,
                        const std::string_view prefix,
                        SourceLocationTp&& loc) noexcept
                : logger_{logger},
                  level_{lvl},
                  loc_{std::forward<SourceLocationTp>(loc)},
                  prefix_{} {
                prefix_.assign(prefix);  // runtime: truncates silently at PrefixNameStorage capacity
            }

            /// Appends value to the accumulated message via the underlying ostringstream.
            template <typename T>
            StreamProxy& operator<<(const T& value) {
                stream_ << value;
                return *this;
            }

            ~StreamProxy() noexcept {
                if (logger_ == nullptr) {
                    return;  // gated out: nothing was ever meant to be published
                }
                thread_local std::string tl_msg_buf;
                tl_msg_buf.clear();
                tl_msg_buf.append(stream_.view());
                const auto meta = EventMeta{level_, loc_};

                const std::int64_t seq = logger_->disruptor_.sequencer().next();
                auto& event            = logger_->disruptor_.ring_buffer()[seq];

                event.message.swap(tl_msg_buf);
                event.prefix.assign(prefix_.view());
                apply_meta(event, meta);

                logger_->disruptor_.sequencer().publish(seq);
            }

        private:
            Logger* logger_;
            LogLevel level_;
            std::source_location loc_;
            PrefixNameStorage prefix_;
            std::ostringstream stream_;
        };

        /// Starts a StreamProxy: publishes the accumulated `<<` output as one event
        /// when the returned proxy is destroyed (typically at the end of the statement).
        template <typename SourceLocationTp = std::source_location>
            requires std::is_same_v<std::remove_cvref_t<SourceLocationTp>, std::source_location>
        StreamProxy
        stream(const LogLevel lvl, std::string_view prefix, SourceLocationTp loc = std::source_location::current()) {
            return StreamProxy{passes_gate(lvl) ? this : nullptr, lvl, prefix, std::forward<SourceLocationTp>(loc)};
        }

        /**
         * @brief Graceful shutdown - waits for all events to be processed.
         *
         * Called automatically by destructor.
         */
        void shutdown();

        /// Posts a flush() call to every sink's strand; does not block for completion.
        void flush() const {
            const auto sinks = snapshot();
            for (const auto& [sink, strand] : *sinks) {
                boost::asio::post(strand, [sink] { sink->flush(); });
            }
        }

        /// Runs one janitor pass now: refreshes the gate and asks every non-Healthy sink
        /// whose backoff has expired to recover.
        void sweep();

        /// Replaces the handler invoked when a sink changes lifecycle state. An empty
        /// callback restores the default: re-log through this Logger, or stderr when no
        /// registered sink would accept the report. The handler runs with sweep_mutex_
        /// already released, so a manual sweep() racing the janitor's tick can invoke it
        /// concurrently with itself; it must tolerate that.
        void set_error_callback(std::function<void(const SinkFailure&)> callback);

    private:
        multithread::Disruptor<LogEvent, multithread::MultiProducerSequencer, multithread::AnyWaitStrategy> disruptor_;
        std::optional<boost::asio::thread_pool> owned_pool_;
        boost::asio::any_io_executor executor_;
        std::jthread consumer_thread_;
        std::atomic<bool> running_{false};

        using SinkTable = std::vector<SinkSlot>;

        /// Registry snapshot. libc++ has no atomic<shared_ptr>, so writers copy-mutate-swap
        /// under registry_mutex_ and readers copy the pointer; registry_version_ lets the
        /// consumer skip the lock while nothing has changed.
        std::shared_ptr<const SinkTable> sinks_{std::make_shared<const SinkTable>()};
        mutable std::mutex registry_mutex_;
        std::atomic<std::uint32_t> registry_version_{0};

        [[nodiscard]] std::shared_ptr<const SinkTable> snapshot() const {
            std::lock_guard lock{registry_mutex_};
            return sinks_;
        }

        /// Backing storage for gate_threshold().
        std::atomic<std::uint8_t> gate_{detail::drop_all_threshold};

        /// Recomputes the gate from a registry table. Called by add_sink()/remove_sink()
        /// (inside their locked section) and by the janitor's sweep. The minimum is taken
        /// over every registered sink including Dead ones -- the gate tracks configured
        /// intent, not current health, so a dead sink recovering does not need it loosened.
        void publish_gate(const SinkTable& table) noexcept {
            std::uint8_t min_threshold = detail::drop_all_threshold;
            for (const auto& [sink, strand] : table) {
                min_threshold = std::min(min_threshold, static_cast<std::uint8_t>(sink->dispatch_hint().threshold));
            }
            gate_.store(min_threshold, std::memory_order_relaxed);
        }

        /// Republishes the gate from the live registry table, under registry_mutex_.
        /// The only path by which the janitor may publish the gate: a snapshot taken
        /// before this call can go stale if add_sink()/remove_sink() commits a newer
        /// table in the meantime, and publishing from that stale snapshot after the
        /// newer one would silently revert the gate. Reading sinks_ and publishing from
        /// it under the same lock add_sink()/remove_sink() already hold means whichever
        /// of them runs last is always the one whose table is actually current.
        void republish_gate_from_registry() {
            std::lock_guard lock{registry_mutex_};
            publish_gate(*sinks_);
        }

        /// True if any registered sink might accept an event at this level. This is the
        /// producer fast path's only work when an event is gated out: inline, it folds
        /// into one relaxed load and a compare the optimizer can hoist across a caller's
        /// logging loop (no_sinks benchmark: 5.2 -> 0.4 ns/attempt vs an opaque call).
        /// always_inline makes that a contract rather than a cost-model outcome; the
        /// gnu:: spelling is the one both clang and gcc honor (clang:: dies under
        /// gcc's -Werror=attributes).
        [[nodiscard, gnu::always_inline]] bool passes_gate(const LogLevel lvl) const noexcept {
            return static_cast<std::uint8_t>(lvl) >= gate_.load(std::memory_order_relaxed);
        }

        /**
         * @brief Pre-captured metadata (built outside the CAS critical path)
         */
        struct EventMeta {
            LogLevel level;
            detail::MetaSource location;
            detail::MetaTimePoint time_point;
            detail::MetaThread tid;
            detail::MetaProcess pid;

            constexpr EventMeta(const LogLevel lvl, const std::source_location& loc) noexcept
                : level{lvl},
                  location{loc} {
            }
        };

        /**
         * @brief Apply pre-captured metadata to ring buffer slot (minimal critical path)
         */
        static void apply_meta(LogEvent& event, const EventMeta& meta) noexcept {
            event.level           = meta.level;
            event.location        = meta.location;
            event.time_point      = meta.time_point;
            event.tid             = meta.tid;
            event.pid             = meta.pid;
            event.shutdown_signal = false;
        }

        /// Shared body behind every publish path: log()'s two overloads (after their gate
        /// check) and default_error_report() (which bypasses the gate -- it has already
        /// proven a sink accepts the report). Builds the event's metadata, claims the next
        /// ring slot, and publishes it. Takes msg by mutable reference and swaps it into
        /// the ring slot rather than copying: callers must pass an owned buffer they no
        /// longer need afterward -- the swap leaves it holding whatever the ring slot's
        /// previous occupant left behind, not msg's original content.
        void publish_event(LogLevel lvl, std::string_view prefix, std::string& msg, const std::source_location& loc);

        /**
         * @brief Starts the consumer thread, then (if configured) the janitor.
         *
         * The janitor must start last: everything janitor_loop()/sweep_once() touch
         * (registry, gate, sweep_mutex_, error_callback_) is fully constructed by the
         * time this runs, since it is called as the constructors' only statement, after
         * every member has already been initialized. Starting it any earlier risks the same
         * half-constructed-member race this codebase has already hit elsewhere.
         *
         * If starting the janitor throws (e.g. EAGAIN under thread exhaustion), the
         * constructor is about to fail and consumer_thread_ is torn down by its own
         * destructor while the exception unwinds. That destructor's request_stop() +
         * join() cannot rely on consumer_loop() noticing the stop request on its own:
         * nothing has been published to a brand-new Logger yet, so the consumer is
         * spinning or parked inside wait_for(), which -- under every wait strategy
         * (BusySpin, Yielding, and Blocking alike) -- only returns once the cursor
         * advances; a stop request by itself never makes that happen, so plain
         * request_stop() + join() would block forever. Catching the failure here and
         * waking the consumer the same way shutdown() does (publishing the shutdown
         * sentinel, which every wait strategy's signal() does wake) before joining it
         * ourselves leaves jthread's own unwind-time destructor with nothing left to do.
         */
        void start_threads(const LoggerConfig& cfg) {
            running_.store(true, std::memory_order_release);
            consumer_thread_ = std::jthread{[this](const std::stop_token& token) { consumer_loop(token); }};

            health_check_interval_ = cfg.health_check_interval();
            if (health_check_interval_.count() <= 0) {
                return;
            }
            try {
                janitor_ = std::jthread{[this](const std::stop_token& token) { janitor_loop(token); }};
            } catch (...) {
                running_.store(false, std::memory_order_release);
                const std::int64_t seq                        = disruptor_.sequencer().next();
                disruptor_.ring_buffer()[seq].shutdown_signal = true;
                disruptor_.sequencer().publish(seq);
                if (consumer_thread_.joinable()) {
                    consumer_thread_.join();
                }
                throw;
            }
        }

        /**
         * @brief Consumer thread loop - processes events and dispatches to sinks
         *
         * @param token Stop token from consumer_thread_'s jthread. Checked alongside
         *              running_ in the loop condition as defense in depth; see
         *              start_threads() for why the janitor-construction-failure path
         *              additionally wakes this loop rather than relying on the token alone.
         */
        void consumer_loop(const std::stop_token& token);

        /**
         * @brief Create wait strategy based on config
         */
        static multithread::AnyWaitStrategy create_wait_strategy(const LoggerConfig::WaitStrategy strategy) {
            using namespace multithread;

            switch (strategy) {
                case LoggerConfig::WaitStrategy::BusySpin:
                    return AnyWaitStrategy::make<BusySpinWaitStrategy>();
                case LoggerConfig::WaitStrategy::Yielding:
                    return AnyWaitStrategy::make<YieldingWaitStrategy>();
                case LoggerConfig::WaitStrategy::Blocking:
                    return AnyWaitStrategy::make<BlockingWaitStrategy>();
                default:
                    return AnyWaitStrategy::make<YieldingWaitStrategy>();
            }
        }

        // Trailing block, deliberately declared last: see start_threads() for why the
        // janitor must start after everything else is constructed.
        std::chrono::milliseconds health_check_interval_{0};
        std::jthread janitor_;
        mutable std::mutex sweep_mutex_;
        std::function<void(const SinkFailure&)> error_callback_;  // guarded by sweep_mutex_

        /// One sink lifecycle transition awaiting report. Owns everything the callback
        /// touches after sweep_mutex_ is released (see sweep_once()): the shared_ptr
        /// keeps the sink alive however the registry or its other owners race, and
        /// reason is the owned copy backing SinkFailure::reason's string_view.
        struct PendingTransition {
            std::shared_ptr<const Sink> sink;
            SinkStatus from;
            SinkStatus to;
            std::string reason;
            std::uint64_t undelivered;
        };

        /// Compares each sink in table against the status recorded at the previous
        /// sweep (Sink::reported_status_), updates it to match, and returns what moved.
        /// Called with sweep_mutex_ held; deliberately does not invoke the error
        /// callback itself -- see sweep_once() for why that happens only after the lock
        /// is released.
        [[nodiscard]] static std::vector<PendingTransition> report_transitions(const SinkTable& table);

        /// Re-logs the transition through this Logger when some registered sink would
        /// actually accept it, and falls back to stderr when none would.
        void default_error_report(const SinkFailure& failure);

        void sweep_once();
        void janitor_loop(const std::stop_token& token);
    };
}  // namespace menagerie::crow
