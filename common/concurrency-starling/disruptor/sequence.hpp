#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>

namespace menagerie::starling {

    /// Atomic operations shared by every sequence alignment. Wait strategies
    /// accept this base by reference; there is no virtual dispatch.
    /// Construct Sequence or WideSequence to give a counter its own storage.
    class AtomicSequence {
    public:
        /// Acquire a position published by another thread.
        [[nodiscard]] std::int64_t get() const noexcept {
            return value_.load(std::memory_order_acquire);
        }

        /// Publish a position after the preceding payload writes or reads.
        void set(const std::int64_t position) noexcept {
            value_.store(position, std::memory_order_release);
        }

        /// Read owner-private progress or a cached position without synchronization.
        [[nodiscard]] std::int64_t get_relaxed() const noexcept {
            return value_.load(std::memory_order_relaxed);
        }

        /// Update an owner-private counter; this does not publish payload changes.
        void set_relaxed(const std::int64_t position) noexcept {
            value_.store(position, std::memory_order_relaxed);
        }

        /// Reserve one position when multiple threads can update this counter.
        [[nodiscard]] std::int64_t increment_and_get() noexcept {
            return value_.fetch_add(1, std::memory_order_acq_rel) + 1;
        }

        /// Reserve a range when multiple threads can claim positions.
        [[nodiscard]] std::int64_t add_and_get(const std::int64_t count) noexcept {
            return value_.fetch_add(count, std::memory_order_acq_rel) + count;
        }

        bool compare_and_set(std::int64_t& expected, const std::int64_t desired) noexcept {
            return value_.compare_exchange_weak(
                expected, desired, std::memory_order_acq_rel, std::memory_order_acquire);
        }

        /// Compatibility spelling for a relaxed read; this is not C++ volatile.
        [[nodiscard]] std::int64_t get_volatile() const noexcept {
            return get_relaxed();
        }

    protected:
        explicit AtomicSequence(const std::int64_t initial_position) noexcept
            : value_{initial_position} {
        }
        ~AtomicSequence() = default;

    private:
        std::atomic<std::int64_t> value_;
    };

    /// Atomic storage aligned and padded to Alignment bytes. The default -1
    /// means no position has been claimed, published, or consumed yet.
    template <std::size_t Alignment>
    class alignas(Alignment) AlignedSequence : public AtomicSequence {
        static_assert(std::has_single_bit(Alignment) && Alignment >= alignof(AtomicSequence),
                      "Sequence alignment must be a power of two supporting atomic int64_t");

    public:
        explicit AlignedSequence(const std::int64_t initial_position = -1) noexcept
            : AtomicSequence{initial_position} {
        }
    };

    using Sequence     = AlignedSequence<std::hardware_destructive_interference_size>;
    /// Double-width storage separates counters from adjacent-line prefetches.
    using WideSequence = AlignedSequence<2 * std::hardware_destructive_interference_size>;

    static_assert(sizeof(Sequence) == std::hardware_destructive_interference_size);
    static_assert(alignof(Sequence) == std::hardware_destructive_interference_size);
    static_assert(sizeof(WideSequence) == 2 * std::hardware_destructive_interference_size);
    static_assert(alignof(WideSequence) == 2 * std::hardware_destructive_interference_size);
}  // namespace menagerie::starling
