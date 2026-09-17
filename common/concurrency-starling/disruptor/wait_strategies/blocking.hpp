#pragma once
#include <condition_variable>
#include <mutex>

#include "wait_strategy.hpp"

namespace menagerie::starling {
    /**
     * @brief Blocking wait strategy - lowest CPU usage, higher latency.
     *
     * ## How It Works
     *
     * ```
     * std::unique_lock lock(mutex);
     * while (cursor.get() < sequence) {
     *     cv.wait(lock);  // BLOCK until notified
     * }
     * ```
     *
     * Thread is removed from scheduler, uses ZERO CPU.
     *
     * ## Condition Variable Explained
     *
     * Producer:
     * ```
     * publish(data);
     * cv.notify_one();  // Wake up one waiting consumer
     * ```
     *
     * Consumer:
     * ```
     * lock(mutex);
     * while (!predicate) { cv.wait(lock); }  // Atomically unlocks and sleeps
     * // Lock re-acquired here
     * process(data);
     * ```
     *
     * What happens in cv.wait():
     * 1. Atomically unlock mutex and add thread to wait queue
     * 2. Thread sleeps (OS blocks it)
     * 3. On notify: OS wakes thread
     * 4. Thread re-acquires mutex
     * 5. Returns from wait()
     *
     * ## Latency Breakdown
     * - notify_one() system call: ~500ns
     * - Wake thread from wait queue: ~1-2us
     * - Thread scheduled by OS: ~1-5us
     * - Mutex re-acquisition: ~100-500ns
     * - Total: ~5-10us worst case
     *
     * ## When To Use
     * - Low message rate (messages per second << 100k)
     * - CPU efficiency more important than latency
     * - Battery-powered devices
     * - Background workers, batch processing
     * - Examples: File rotation, periodic flushing, monitoring
     *
     * ## When NOT To Use
     * - High throughput (mutex contention)
     * - Sub-microsecond latency required
     */
    class BlockingWaitStrategy final {
    public:
        /// Blocks on a condition variable until `cursor` reaches `sequence`; uses zero
        /// CPU while parked.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence, const AtomicSequence& cursor) const noexcept {
            std::int64_t available_sequence;

            // Quick check before locking (optimization)
            if ((available_sequence = cursor.get()) >= sequence) {
                return available_sequence;
            }

            // Need to wait - acquire lock and use condition variable
            std::unique_lock lock{mutex_};

            // Spurious wakeup loop (cv can wake without notify!)
            while ((available_sequence = cursor.get()) < sequence) {
                cv_.wait(lock);  // Unlock, sleep, wake, re-lock
            }

            return available_sequence;
        }

        /// Wakes one waiting thread via the condition variable.
        void signal() const noexcept {
            // Pair with the waiter's predicate check and atomic unlock/wait.
            // Publication precedes this lock; a notification cannot slip between
            // the last false predicate check and entry into the wait.
            std::lock_guard lock{mutex_};
            cv_.notify_one();
        }

        /// Wakes every waiting thread (e.g. for shutdown).
        void signal_all() const noexcept {
            std::lock_guard lock{mutex_};
            cv_.notify_all();
        }

    private:
        mutable std::mutex mutex_;
        mutable std::condition_variable cv_;
    };
}  // namespace menagerie::starling
