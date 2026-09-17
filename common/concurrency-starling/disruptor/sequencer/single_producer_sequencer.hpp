#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>

#include <pause.hpp>

#include "sequence.hpp"
#include "sequencer_concept.hpp"
#include "shared/constants.hpp"
#include "wait_strategies/wait_strategy.hpp"

namespace menagerie::starling {

    /**
     * @brief Single-producer / single-consumer sequencer - the SPSC fast path.
     *
     * Because exactly one producer claims and publishes **in order**, two costs of the
     * multi-producer path disappear:
     *   - no CAS / fetch-add on claim: the producer owns claimed_sequence_ and
     *     advances its atomic value with relaxed load/store;
     *   - no per-slot availability buffer - there can be no gaps, so the cursor *is* the
     *     published frontier and `get_published_sequence()` is a single acquire load.
     *
     * Contract: a single producer thread calls next()/next_batch()/try_next()/publish()
     * and must publish in claim order. Using it from multiple producers is undefined.
     *
     * @tparam WaitStrategyT Wait strategy (stored by value, statically dispatched - no vtable).
     */
    template <IsWaitStrategy WaitStrategyT>
    class SingleProducerSequencer {
    public:
        /**
         * @brief Construct over a runtime (power-of-2) buffer size.
         * @param buffer_size Ring size; must be a non-zero power of 2 (debug-asserted).
         * @param wait_strategy_args Forwarded to construct the wait strategy in place.
         */
        template <typename... WaitStrategyArgsTp>
        explicit SingleProducerSequencer(const std::size_t buffer_size,
                                         WaitStrategyArgsTp&&... wait_strategy_args) noexcept
            : cached_capacity_limit_{static_cast<std::int64_t>(buffer_size - 1)},
              capacity_{buffer_size},
              wait_strategy_{std::forward<WaitStrategyArgsTp>(wait_strategy_args)...} {
            assert(buffer_size != 0 && std::has_single_bit(buffer_size) && "Buffer size must be a non-zero power of 2");
        }

        // ----------------------------------------------------------------- claim
        /// @brief Claim the next sequence (blocks if the buffer is full).
        [[nodiscard]] std::int64_t next() {
            const std::int64_t next = claimed_sequence_.get_relaxed() + 1;
            wait_for_capacity(next);
            claimed_sequence_.set_relaxed(next);
            return next;
        }

        /// @brief Claim n contiguous sequences; returns the first.
        [[nodiscard]] std::int64_t next_batch(const std::int64_t count) {
            const auto previous_claim = claimed_sequence_.get_relaxed();
            const auto last_claim     = previous_claim + count;
            wait_for_capacity(last_claim);
            claimed_sequence_.set_relaxed(last_claim);
            return previous_claim + 1;
        }

        /// @brief Best-effort single claim; returns -1 if the buffer is full (no CAS needed).
        [[nodiscard]] std::int64_t try_next() noexcept {
            const std::int64_t next = claimed_sequence_.get_relaxed() + 1;
            if (next - static_cast<std::int64_t>(capacity_) > consumed_sequence_.get()) {
                return -1;  // would block
            }
            claimed_sequence_.set_relaxed(next);
            return next;
        }

        // --------------------------------------------------------------- publish
        /// @brief Mark a sequence published (release): its data is now visible.
        void publish(const std::int64_t sequence) noexcept {
            published_sequence_.set(sequence);  // release store; in-order -> published frontier
            wait_strategy_.signal();
        }
        /// @brief Mark an inclusive range [lo, hi] published (in-order -> just advance to hi).
        void publish_batch([[maybe_unused]] const std::int64_t lo, const std::int64_t hi) noexcept {
            published_sequence_.set(hi);
            wait_strategy_.signal();
        }

        // -------------------------------------------------------------- consumer
        /// @brief Highest contiguous published sequence at or after lower_bound.
        /// A single in-order producer cannot leave publication gaps.
        [[nodiscard]] std::int64_t
        get_published_sequence([[maybe_unused]] const std::int64_t lower_bound) const noexcept {
            return published_sequence_.get();
        }

        /// True iff `sequence` has been published (is at or behind the cursor).
        [[nodiscard]] bool is_available(const std::int64_t sequence) const noexcept {
            return published_sequence_.get() >= sequence;
        }

        /// @brief Release completed reads through sequence, allowing slot reuse.
        /// The single consumer must finish all reads through this position and
        /// advance it monotonically; this acknowledges reads, it does not perform them.
        void consume(const std::int64_t sequence) noexcept {
            consumed_sequence_.set(sequence);
        }

        /// @brief Release the completed inclusive range [lo, hi] with one release store.
        /// The single consumer must finish every read in the range and all earlier
        /// reads first. In-order consumption needs only hi to advance the position.
        void consume_batch([[maybe_unused]] const std::int64_t lo, const std::int64_t hi) noexcept {
            consume(hi);
        }

        /// @brief Block (per the wait strategy) until `sequence` is published.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence) {
            return wait_strategy_.wait_for(sequence, published_sequence_);
        }
        /// @brief Wake all waiters (e.g. for shutdown).
        void signal_all() noexcept {
            wait_strategy_.signal_all();
        }

        // ------------------------------------------------------------ accessors
        /// Highest sequence a consumer has marked consumed (drives backpressure).
        [[nodiscard]] std::int64_t get_consumed_sequence() const noexcept {
            return consumed_sequence_.get();
        }
        /// @brief Approximate free slots based on published/consumed positions.
        /// Unpublished claims are not reflected; use next()/try_next() to reserve.
        [[nodiscard]] std::int64_t remaining_capacity() const noexcept {
            return static_cast<std::int64_t>(capacity_) - (published_sequence_.get() - consumed_sequence_.get());
        }
        /// Approximate number of published items waiting to be consumed.
        [[nodiscard]] std::int64_t approx_size() const noexcept {
            const auto consumed = consumed_sequence_.get();
            return published_sequence_.get() - consumed;
        }

    private:
        void wait_for_capacity(const std::int64_t sequence) {
            if (sequence > cached_capacity_limit_.get_relaxed()) {
                // Capacity arithmetic is needed only when the cached limit runs out.
                const std::int64_t wrap_point = sequence - static_cast<std::int64_t>(capacity_);
                std::int64_t consumed;
                std::int16_t spin_count = 0;
                while (wrap_point > (consumed = consumed_sequence_.get())) {
                    if (++spin_count < SPIN_BEFORE_YIELD) {
                        pause_arc_agnostic();
                    } else {
                        std::this_thread::yield();
                        spin_count = 0;
                    }
                }
                // All valid claims fit int64_t. Saturating a larger unsigned
                // limit preserves the capacity check and fits the same wrapper.
                const auto limit = static_cast<std::uint64_t>(consumed) + capacity_;
                cached_capacity_limit_.set_relaxed(static_cast<std::int64_t>(
                    std::min(limit, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))));
            }
        }

        // Producer release-stores after filling slots; the consumer acquires
        // this inclusive frontier to know which payloads are ready.
        WideSequence published_sequence_;
        // Consumer release-stores after reading slots; the producer acquires
        // this inclusive frontier before reusing their storage.
        WideSequence consumed_sequence_;
        // Highest reservation made by the sole producer. Private to that thread:
        // relaxed load/store is enough, and no atomic RMW is needed.
        WideSequence claimed_sequence_;
        // Producer-private upper bound for claims that fit without re-reading
        // consumption: min(last observed consumed + capacity, INT64_MAX).
        Sequence cached_capacity_limit_;
        std::size_t capacity_;
        WaitStrategyT wait_strategy_;
    };

    static_assert(IsSequencer<SingleProducerSequencer<AnyWaitStrategy>>,
                  "SingleProducerSequencer must satisfy the Sequencer concept");

}  // namespace menagerie::starling
