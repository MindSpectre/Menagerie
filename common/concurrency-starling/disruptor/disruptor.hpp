#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <menagerie/beaver>
#include <type_traits>
#include <utility>

#include "ring_buffer/ring_buffer.hpp"
#include "ring_buffer/static_ring_buffer.hpp"
#include "sequencer/multi_producer_sequencer.hpp"
#include "sequencer/sequencer_concept.hpp"
#include "sequencer/single_producer_sequencer.hpp"
#include "wait_strategies/blocking.hpp"
#include "wait_strategies/busy_spin.hpp"
#include "wait_strategies/timeout_blocking.hpp"
#include "wait_strategies/wait_strategy.hpp"
#include "wait_strategies/yielding_strategy.hpp"

/**
 * @file disruptor.hpp
 * @brief Ring-buffer messaging with queue conveniences and explicit sequencing.
 *
 * `Disruptor<T>` defaults to multiple producers, one consumer, and busy spinning.
 * Select `SingleProducerSequencer` when exactly one thread produces:
 * ```cpp
 * Disruptor<Event, SingleProducerSequencer> queue{1024};
 * queue.emplace(event_id, payload);  // producer: construct and publish
 * Event event = queue.pull();       // consumer: move out and release one slot
 * ```
 *
 * The ring preconstructs its slots, so T must be default-constructible; it can be
 * move-only. emplace() requires nonthrowing destruction and either nonthrowing
 * construction or a nonthrowing move for staging before reservation. pull()
 * requires nonthrowing move construction and destruction.
 *
 * Advanced producers can still claim and publish batches via sequencer() and
 * fill slots through ring_buffer(). A single producer must publish existing
 * claims in order before using emplace(). pull() and manual consumption share
 * the consumed position; call consume()/consume_batch() in order after manual reads.
 * Multi-producer publication gaps are checked explicitly, even with a blocking
 * wait strategy: the MP claim cursor alone does not make a payload readable.
 */
namespace menagerie::starling {

    /**
     * @brief Lock-free ring-buffer messaging pipeline (LMAX Disruptor pattern):
     *        bundles a runtime-sized ring buffer with a sequencer and a
     *        compile-time wait strategy.
     */
    template <typename T,
              template <typename> class SequencerT = MultiProducerSequencer,
              IsWaitStrategy WaitStrategyT         = BusySpinWaitStrategy>
        requires IsSequencer<SequencerT<WaitStrategyT>>
    class Disruptor : beaver::Immutable {
    public:
        /**
         * @param buffer_size Ring size; must be a non-zero power of 2.
         * @param ws_args     Forwarded to construct the wait strategy in place (e.g.
         *                    `Disruptor<T, MultiProducerSequencer, TimeoutBlockingWaitStrategy>{n, 100ms}`).
         */
        template <typename... WaitStrategyArgsT>
        explicit Disruptor(const std::size_t buffer_size, WaitStrategyArgsT&&... ws_args) noexcept
            : sequencer_{buffer_size, std::forward<WaitStrategyArgsT>(ws_args)...},
              ring_buffer_{buffer_size} {
        }

        /// Construct and publish one item, waiting for capacity when full.
        /// The sequencer determines whether one or multiple producers are allowed.
        /// A throwing construction happens before reservation, then moves into the
        /// slot without throwing; a noexcept construction happens directly in place.
        template <typename... Args>
            requires(std::is_constructible_v<T, Args && ...> && std::is_nothrow_destructible_v<T> &&
                     (std::is_nothrow_constructible_v<T, Args && ...> || std::is_nothrow_move_constructible_v<T>))
        void emplace(Args&&... args) {
            if constexpr (std::is_nothrow_constructible_v<T, Args&&...>) {
                const auto sequence = sequencer_.next();
                auto* slot          = std::addressof(ring_buffer_[sequence]);
                std::destroy_at(slot);
                std::construct_at(slot, std::forward<Args>(args)...);
                sequencer_.publish(sequence);
            } else {
                // Never leave an unpublished claim if user construction throws.
                T value(std::forward<Args>(args)...);
                emplace(std::move(value));
            }
        }

        /// Return the next published item, waiting when empty. Exactly one thread
        /// may consume. The next position follows the sequencer's consumed position,
        /// including completed manual reads acknowledged by consume()/consume_batch().
        /// Slots stay alive in their moved-from state until reuse or destruction.
        [[nodiscard]] T pull()
            requires(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>)
        {
            const auto sequence = sequencer_.get_consumed_sequence() + 1;
            if (sequence > cached_published_sequence_.get_relaxed()) {
                auto published = sequencer_.wait_for(sequence);
                while (published < sequence) {
                    // A timeout or delayed MP publisher can leave this sequence unavailable.
                    pause_arc_agnostic();
                    published = sequencer_.wait_for(sequence);
                }
                cached_published_sequence_.set_relaxed(published);
            }
            T value = std::move(ring_buffer_[sequence]);
            sequencer_.consume(sequence);
            return value;
        }

        /// The sequencer coordinating claim/publish/consume.
        [[nodiscard]] SequencerT<WaitStrategyT>& sequencer() noexcept {
            return sequencer_;
        }
        /// @copydoc sequencer
        [[nodiscard]] const SequencerT<WaitStrategyT>& sequencer() const noexcept {
            return sequencer_;
        }

        /// The underlying ring buffer.
        [[nodiscard]] RingBuffer<T>& ring_buffer() noexcept {
            return ring_buffer_;
        }
        /// @copydoc ring_buffer
        [[nodiscard]] const RingBuffer<T>& ring_buffer() const noexcept {
            return ring_buffer_;
        }

    private:
        // Reuse the sequencer's trailing metadata padding for ring metadata.
        // Individual WideSequence objects retain their full spacing.
        [[no_unique_address]] SequencerT<WaitStrategyT> sequencer_;
        RingBuffer<T> ring_buffer_;
        // Consumer-owned snapshot of acquired contiguous publication. It can lag
        // the producer and avoids reading its shared cursor on every pull.
        WideSequence cached_published_sequence_;
    };

}  // namespace menagerie::starling
