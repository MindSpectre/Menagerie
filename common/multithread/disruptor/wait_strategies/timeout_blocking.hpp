#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace menagerie::multithread {

    /**
     * @brief Timeout-based blocking strategy with configurable timeout.
     *
     * Like BlockingWaitStrategy but wakes periodically to check shutdown flags.
     * Useful for graceful shutdown without explicit signaling.
     */
    class TimeoutBlockingWaitStrategy final {
    public:
        /// @param timeout Wake-and-recheck period while parked (e.g. for shutdown polling).
        explicit TimeoutBlockingWaitStrategy(
            const std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) noexcept
            : timeout_{timeout} {
        }

        /// Blocks on a condition variable, waking every `timeout_` to recheck `cursor`
        /// even without an explicit signal.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor) const {
            std::int64_t available_sequence;

            if ((available_sequence = cursor.get()) >= sequence) {
                return available_sequence;
            }

            std::unique_lock lock{mutex_};
            available_sequence = cursor.get();
            while (available_sequence < sequence) {
                // Wait with timeout - returns on timeout OR notify
                cv_.wait_for(lock, timeout_);

                // Check again (might be timeout, not notify)
                available_sequence = cursor.get();
            }

            return available_sequence;
        }

        /// Wakes one waiting thread via the condition variable.
        void signal() const noexcept {
            cv_.notify_one();
        }

        /// Wakes every waiting thread (e.g. for shutdown).
        void signal_all() const noexcept {
            cv_.notify_all();
        }

    private:
        mutable std::mutex mutex_{};
        mutable std::condition_variable cv_{};
        std::chrono::milliseconds timeout_;
    };
}  // namespace menagerie::multithread
