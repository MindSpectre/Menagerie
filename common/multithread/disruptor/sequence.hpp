#pragma once

#include <atomic>
#include <menagerie/beavers>
#include <new>

namespace menagerie::multithread {

    /**
     * @brief Cache-line aligned atomic sequence counter.
     *
     * ## Concept: False Sharing Prevention
     *
     * Modern CPUs organize memory into cache lines (64 bytes on x86-64; up to 256
     * on some AArch64 implementations - hence the standard constants below).
     * When multiple threads access different variables on the same cache line,
     * the CPU must invalidate and reload the entire line even though threads
     * aren't actually sharing data. This is called "false sharing" and kills performance.
     *
     * Example of FALSE SHARING (BAD):
     * ```
     * struct Bad {
     *     std::atomic<int64_t> producer_cursor;  // Bytes 0-7
     *     std::atomic<int64_t> consumer_cursor;  // Bytes 8-15  (SAME CACHE LINE!)
     * };
     * ```
     * When producer updates its cursor, consumer's cache line gets invalidated!
     *
     * Solution: Align each sequence to its own cache line, using
     * `std::hardware_destructive_interference_size` - the standard's name for
     * "minimum offset that keeps two objects off each other's cache line":
     * ```
     * struct alignas(std::hardware_destructive_interference_size) Good {
     *     std::atomic<int64_t> producer_cursor;  // Cache line 0
     *     char padding[...];                     // Pad out the rest of the line
     * };
     * struct alignas(std::hardware_destructive_interference_size) Good2 {
     *     std::atomic<int64_t> consumer_cursor;  // Cache line 1 (SEPARATE!)
     * };
     * ```
     *
     * ## Performance Impact
     * - With false sharing: ~50-100ns per atomic operation (cache coherency overhead)
     * - With proper alignment: ~5-10ns per atomic operation (L1 cache hit)
     * - **10-20x performance improvement!**
     *
     * ## Memory Ordering
     * - `relaxed`: No synchronization, just atomic read/write
     * - `acquire`: All reads after this see writes before the release
     * - `release`: All writes before this are visible to acquire
     * - `seq_cst`: Total global order (expensive, avoid on hot path)
     */
    class alignas(std::hardware_destructive_interference_size) Sequence {
    public:
        /**
         * @brief Initialize sequence with starting value
         * @param initial_value Starting sequence number (default: -1 for "uninitialized")
         *
         * Note: -1 represents "nothing claimed/consumed yet".
         * First valid sequence after increment is 0.
         */
        explicit Sequence(const std::int64_t initial_value = -1) noexcept
            : value_{initial_value} {
            beavers::unused_value(padding_);
        }

        /**
         * @brief Get current sequence value
         * @return Current sequence number
         *
         * Uses acquire ordering to ensure we see all writes that happened-before
         * the corresponding release store.
         */
        [[nodiscard]] std::int64_t get() const noexcept {
            return value_.load(std::memory_order_acquire);
        }

        /**
         * @brief Set sequence to specific value
         * @param new_value New sequence number
         *
         * Uses release ordering to ensure all our previous writes are visible
         * to threads that do acquire loads.
         */
        void set(const std::int64_t new_value) noexcept {
            value_.store(new_value, std::memory_order_release);
        }

        /**
         * @brief Atomically increment and return new value
         * @return New value after increment
         *
         * fetch_add returns the OLD value, so we add 1 to get the new value.
         * Uses acq_rel: acts as both acquire (for reading) and release (for writing).
         */
        [[nodiscard]] std::int64_t increment_and_get() noexcept {
            return value_.fetch_add(1, std::memory_order_acq_rel) + 1;
        }

        /**
         * @brief Atomically add delta and return new value
         * @param delta Amount to add
         * @return New value after addition
         */
        [[nodiscard]] std::int64_t add_and_get(const std::int64_t delta) noexcept {
            return value_.fetch_add(delta, std::memory_order_acq_rel) + delta;
        }

        /**
         * @brief Compare-and-swap operation
         * @param expected Expected current value (updated with actual value if CAS fails)
         * @param desired New value to set if current == expected
         * @return true if swap succeeded, false otherwise
         *
         * Classic lock-free primitive:
         * - If value == expected: atomically set value = desired, return true
         * - If value != expected: update expected with actual value, return false
         *
         * Example usage (claim a sequence):
         * ```
         * std::int64_t current = cursor.get();
         * std::int64_t next = current + 1;
         * while (!cursor.compare_and_set(current, next)) {
         *     // CAS failed (another thread claimed it), retry with updated current
         *     next = current + 1;
         * }
         * return next;  // Successfully claimed sequence 'next'
         * ```
         */
        bool compare_and_set(std::int64_t& expected, const std::int64_t desired) noexcept {
            return value_.compare_exchange_weak(
                expected, desired, std::memory_order_acq_rel, std::memory_order_acquire);
        }

        /**
         * @brief Volatile read (for debugging/testing)
         * @return Current value without any ordering guarantees
         *
         * WARNING: Only use for non-critical reads (e.g., logging, metrics)
         */
        [[nodiscard]] std::int64_t get_volatile() const noexcept {
            return value_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<std::int64_t> value_;

        // Padding so the sequence occupies a full cache line on its own
        char padding_[std::hardware_destructive_interference_size - sizeof(std::atomic<std::int64_t>)]{};
    };

    // Compile-time verification that our alignment worked
    static_assert(sizeof(Sequence) == std::hardware_destructive_interference_size,
                  "Sequence must be exactly one cache line");
    static_assert(alignof(Sequence) == std::hardware_destructive_interference_size,
                  "Sequence must be cache-line aligned");

}  // namespace menagerie::multithread
