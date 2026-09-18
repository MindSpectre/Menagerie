#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include <pause.hpp>

#include "sequence.hpp"
#include "sequencer_concept.hpp"
#include "shared/constants.hpp"
#include "wait_strategies/wait_strategy.hpp"

namespace menagerie::starling {

    /**
     * @brief Multi-producer sequencer with a claim/publish protocol.
     *
     * ## Claiming: lock-free fetch-add
     *
     * `next()` advances the shared cursor with a single atomic fetch-add, so every
     * producer gets a unique sequence in one instruction with no CAS-retry under
     * contention. The cursor therefore tracks the highest *claimed* sequence and may
     * run ahead of what is actually published. get_published_sequence() scans the
     * availability flags up to the claimed position and stops at the first gap.
     * (`try_next()` keeps a single-shot CAS: its contract is to *not* claim
     * when full, which fetch-add cannot model.)
     *
     * ## Availability: rotation numbers, never cleared
     *
     * Each ring slot stores the *generation* of the last sequence published into it
     * (`sequence >> index_shift`), not a boolean. A slot is "published for seq" iff it
     * holds `generation_of(seq)`. When a slot is reused a wrap later, the publisher
     * overwrites it with the new generation, so a stale value from the previous
     * generation never looks available. This needs no consumer-side write-back
     * (no mark_consumed), so the flag cache lines flow one way (producer -> consumer).
     * Backpressure (the capacity wait below) still guarantees a slot's previous occupant
     * has been consumed before it is overwritten.
     *
     * @tparam WaitStrategyT Wait strategy (stored by value, statically dispatched - no vtable).
     */
    template <IsWaitStrategy WaitStrategyT>
    class MultiProducerSequencer {
    public:
        /**
         * @brief Construct over a runtime (power-of-2) buffer size.
         * @param buffer_size Ring size; must be a non-zero power of 2 (debug-asserted).
         * @param wait_strategy_args     Forwarded to construct the wait strategy in place
         *                    (supports non-movable strategies such as Blocking).
         */
        template <typename... WaitStrategyArgsTp>
        explicit MultiProducerSequencer(const std::size_t buffer_size,
                                        WaitStrategyArgsTp&&... wait_strategy_args) noexcept
            : capacity_{buffer_size},
              slot_index_mask_{buffer_size - 1},
              generation_shift_{std::countr_zero(buffer_size)},
              wait_strategy_{std::forward<WaitStrategyArgsTp>(wait_strategy_args)...},
              slot_generations_{buffer_size} {
            assert(buffer_size != 0 && std::has_single_bit(buffer_size) && "Buffer size must be a non-zero power of 2");
        }

        // ----------------------------------------------------------------- claim
        /// @brief Claim the next sequence (blocks if the buffer is full).
        [[nodiscard]] std::int64_t next() {
            const std::int64_t next = claimed_sequence_.increment_and_get();
            wait_for_capacity(next);
            return next;
        }

        /// @brief Claim n contiguous sequences in one fetch-add; returns the first.
        [[nodiscard]] std::int64_t next_batch(const std::int64_t n) {
            const std::int64_t last = claimed_sequence_.add_and_get(n);  // highest claimed in the block
            wait_for_capacity(last);
            return last - n + 1;
        }

        /// @brief Best-effort single claim; returns -1 if full or contended (CAS).
        [[nodiscard]] std::int64_t try_next() noexcept {
            std::int64_t current    = claimed_sequence_.get();
            const std::int64_t next = current + 1;
            if (const std::int64_t wrap_point = next - static_cast<std::int64_t>(capacity_);
                wrap_point > consumed_sequence_.get()) {
                return -1;  // would block
            }
            if (claimed_sequence_.compare_and_set(current, next)) {
                return next;
            }
            return -1;  // another producer claimed it
        }

        // --------------------------------------------------------------- publish
        /// @brief Mark a sequence published (release): its data is now visible.
        void publish(const std::int64_t sequence) noexcept {
            publish_slot(sequence);
            wait_strategy_.signal();
        }
        /// @brief Mark an inclusive range [lo, hi] published.
        void publish_batch(const std::int64_t lo, const std::int64_t hi) noexcept {
            for (std::int64_t seq = lo; seq <= hi; ++seq) {
                publish_slot(seq);
            }
            wait_strategy_.signal();
        }

        // -------------------------------------------------------------- consumer
        /// @brief Highest contiguous published sequence beginning at lower_bound.
        /// The claimed position is only the scan limit; publication gaps remain hidden.
        [[nodiscard]] std::int64_t get_published_sequence(const std::int64_t lower_bound) const noexcept {
            return scan_published(lower_bound, claimed_sequence_.get());
        }

        /// True iff `sequence` has been published (its slot holds the matching generation).
        [[nodiscard]] bool is_available(const std::int64_t sequence) const noexcept {
            return slot_generations_[static_cast<std::size_t>(sequence) & slot_index_mask_].generation.load(
                       std::memory_order_acquire) == generation_of(sequence);
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

        /// @brief Wait for a claim, then return the contiguous published frontier.
        /// A timeout or publication gap can return a value below sequence.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence) {
            const auto claimed = wait_strategy_.wait_for(sequence, claimed_sequence_);
            if (claimed < sequence) {
                return claimed;
            }
            return scan_published(sequence, claimed);
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
        /// @brief Free slots. May read negative: with fetch-add the cursor can race
        /// ahead of the consumed sequence before producers publish/consumers catch up.
        [[nodiscard]] std::int64_t remaining_capacity() const noexcept {
            return static_cast<std::int64_t>(capacity_) - (claimed_sequence_.get() - consumed_sequence_.get());
        }
        /// Approximate number of claimed items not yet consumed. Unpublished
        /// reservations count, so contention can transiently take this above capacity.
        [[nodiscard]] std::int64_t approx_size() const noexcept {
            const auto consumed = consumed_sequence_.get();
            return claimed_sequence_.get() - consumed;
        }

    private:
        [[nodiscard]] std::int64_t scan_published(const std::int64_t lower_bound,
                                                  const std::int64_t claimed_sequence) const noexcept {
            for (std::int64_t seq = lower_bound; seq <= claimed_sequence; ++seq) {
                if (slot_generations_[static_cast<std::size_t>(seq) & slot_index_mask_].generation.load(
                        std::memory_order_acquire) != generation_of(seq)) {
                    return seq - 1;
                }
            }
            return claimed_sequence;
        }

        [[nodiscard]] std::int32_t generation_of(const std::int64_t sequence) const noexcept {
            return static_cast<std::int32_t>(sequence >> generation_shift_);
        }

        void publish_slot(const std::int64_t sequence) noexcept {
            slot_generations_[static_cast<std::size_t>(sequence) & slot_index_mask_].generation.store(
                generation_of(sequence), std::memory_order_release);
        }

        void wait_for_capacity(const std::int64_t sequence) const {
            const std::int64_t wrap_point = sequence - static_cast<std::int64_t>(capacity_);
            std::int16_t spin_count       = 0;
            while (wrap_point > consumed_sequence_.get()) {
                if (++spin_count < SPIN_BEFORE_YIELD) {
                    pause_arc_agnostic();
                } else {
                    std::this_thread::yield();
                    spin_count = 0;
                }
            }
        }

        // Packed (not per-line aligned): consumer only reads these, producer only
        // writes them, so they intentionally share cache lines like LMAX's int[].
        struct SlotGeneration {
            std::atomic<std::int32_t> generation{-1};  // -1 = never published
        };

        // Shared by producers: RMW reserves unique positions. A claim is not
        // publication; slot_generations_ establishes each payload's availability.
        Sequence claimed_sequence_;
        // Consumer release-stores completed reads; producers acquire this before
        // reusing a slot, including after wraparound.
        Sequence consumed_sequence_;
        std::size_t capacity_;
        std::size_t slot_index_mask_;    // capacity_ - 1
        std::int32_t generation_shift_;  // log2(capacity_)
        WaitStrategyT wait_strategy_;
        std::vector<SlotGeneration> slot_generations_;
    };

    static_assert(IsSequencer<MultiProducerSequencer<AnyWaitStrategy>>,
                  "MultiProducerSequencer must satisfy the Sequencer concept");

}  // namespace menagerie::starling
