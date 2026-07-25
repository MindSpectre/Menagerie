#pragma once

namespace menagerie::multithread {

    /// Configuration for a `ThreadPool`: worker count bounds, idle reaping,
    /// and cleanup-thread cadence. `ok()` validates it before construction.
    struct ThreadPoolConfig {
        std::size_t min_threads{2};  ///< Workers created at construction and never reaped below.
        std::size_t max_threads{4};  ///< Upper bound on concurrently live workers.
        std::chrono::milliseconds idle_timeout{std::chrono::seconds{30}};       ///< Idle time before a worker above `min_threads` is reaped.
        std::chrono::milliseconds cleanup_interval{std::chrono::seconds{15}};   ///< Period of the background cleanup thread.
        bool enable_cleanup_thread{true};  ///< If false, idle workers above min_threads are never reaped.

        /// True iff `min_threads`, `max_threads` are positive and `min_threads <= max_threads`.
        [[nodiscard]] bool ok() const {
            return min_threads > 0 && max_threads > 0 && min_threads <= max_threads;
        }

        /// One worker, no cleanup thread, short timeouts - tests and low-traffic tools.
        static ThreadPoolConfig minimal() {
            return ThreadPoolConfig{.min_threads           = 1,
                                    .max_threads           = 1,
                                    .idle_timeout          = std::chrono::seconds{1},
                                    .cleanup_interval      = std::chrono::seconds{1},
                                    .enable_cleanup_thread = false};
        }

        /// 2-4 workers, moderate idle timeout - general-purpose default.
        static ThreadPoolConfig basic() {
            return ThreadPoolConfig{.min_threads           = 2,
                                    .max_threads           = 4,
                                    .idle_timeout          = std::chrono::milliseconds{500},
                                    .cleanup_interval      = std::chrono::seconds{1},
                                    .enable_cleanup_thread = true};
        }

        /// 4-16 workers, longer idle timeout - sustained high-throughput workloads.
        static ThreadPoolConfig high_performance() {
            return ThreadPoolConfig{.min_threads           = 4,
                                    .max_threads           = 16,
                                    .idle_timeout          = std::chrono::seconds{10},
                                    .cleanup_interval      = std::chrono::seconds{30},
                                    .enable_cleanup_thread = true};
        }

        /// 2-8 workers, short idle timeout and cleanup interval - reaps idle
        /// workers aggressively for bursty workloads.
        static ThreadPoolConfig quick_cleanup() {
            return ThreadPoolConfig{.min_threads           = 2,
                                    .max_threads           = 8,
                                    .idle_timeout          = std::chrono::milliseconds{200},
                                    .cleanup_interval      = std::chrono::milliseconds{500},
                                    .enable_cleanup_thread = true};
        }
    };
}  // namespace menagerie::multithread
