#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <menagerie/beaver>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>

#include "detail/async_acquire.hpp"
#include "detail/deadline.hpp"
#include "detail/lifetime.hpp"
#include "detail/reserved_slot.hpp"
#include "detail/sync_acquire.hpp"
#include "detail/waiter.hpp"
#include "types.hpp"

namespace menagerie::starling {

    /// Passive fixed arena. Construction and publication are explicit transactions.
    /// The pool must outlive all tokens, calls, awaitables and completions.
    template <typename T>
    class Pool : beaver::Immutable {
        static_assert(std::is_nothrow_destructible_v<T>);

    public:
        using value_type = T;

        using Borrowed     = detail::PoolBorrowed<T>;
        using ReservedSlot = detail::PoolReservedSlot<T>;

        explicit Pool(const std::size_t capacity) {
            if (capacity == 0) {
                throw std::invalid_argument{"Pool: capacity must be non-zero"};
            }
            storage_ = std::make_unique<Slot[]>(capacity);
            states_  = std::vector<SlotState>(capacity, SlotState::vacant);
            words_   = std::vector<PaddedWord>(capacity / bits_per_word + (capacity % bits_per_word != 0));
            vacant_.reserve(capacity);
            quarantined_.reserve(capacity);
            for (std::size_t i = capacity; i > 0; --i) {
                vacant_.push_back(i - 1);
            }
        }

        ~Pool() noexcept {
            shutdown();
            async_operations_.assert_drained();
            assert(reserved_count_ == 0 && "Pool destroyed with outstanding reservations");
            assert(waiters_.empty() && "Pool destroyed with outstanding waiters");
            for (std::size_t i = 0; i < states_.size(); ++i) {
                assert(states_[i] != SlotState::borrowed && "Pool destroyed with outstanding borrows");
                if (states_[i] == SlotState::idle || states_[i] == SlotState::quarantined) {
                    std::destroy_at(slot_ptr(i));
                }
            }
        }

        [[nodiscard]] std::expected<Borrowed, AcquireError> try_acquire() {
            if (closed_.load(std::memory_order_acquire)) {
                return std::unexpected{AcquireError::shutdown};
            }
            const auto index = claim_idle();
            if (!index) {
                return std::unexpected{closed_.load(std::memory_order_acquire) ? AcquireError::shutdown
                                                                              : AcquireError::exhausted};
            }
            if (closed_.load(std::memory_order_acquire)) {
                std::lock_guard lock{mutex_};
                quarantine_borrowed(*index);
                return std::unexpected{AcquireError::shutdown};
            }
            return Borrowed{this, *index};
        }

        [[nodiscard]] std::expected<Borrowed, AcquireError> acquire() {
            return detail::PoolSyncAcquire<T>::acquire(this, std::chrono::steady_clock::time_point::max());
        }

        [[nodiscard]] std::expected<Borrowed, AcquireError>
        acquire_for(const std::chrono::steady_clock::duration timeout) {
            const auto deadline = detail::pool_deadline_after(std::chrono::steady_clock::now(), timeout);
            return detail::PoolSyncAcquire<T>::acquire(this, deadline);
        }

        [[nodiscard]] boost::asio::awaitable<std::expected<Borrowed, AcquireError>> async_acquire() {
            return detail::PoolAsyncAcquire<T>::acquire_until(detail::AsyncPoolReference{this},
                                                              std::chrono::steady_clock::time_point::max());
        }

        [[nodiscard]] boost::asio::awaitable<std::expected<Borrowed, AcquireError>>
        async_acquire_for(const std::chrono::steady_clock::duration timeout) {
            // This wrapper is intentionally not a coroutine: both the deadline
            // and operation-frame diagnostic are captured when the API is called, even
            // when the returned awaitable is started much later.
            const auto deadline = detail::pool_deadline_after(std::chrono::steady_clock::now(), timeout);
            return detail::PoolAsyncAcquire<T>::acquire_until(detail::AsyncPoolReference{this}, deadline);
        }

        [[nodiscard]] std::optional<ReservedSlot> try_reserve() {
            std::lock_guard lock{mutex_};
            if (closed_.load(std::memory_order_relaxed) || vacant_.empty()) {
                return std::nullopt;
            }
            const std::size_t index = vacant_.back();
            vacant_.pop_back();
            assert(states_[index] == SlotState::vacant);
            states_[index] = SlotState::reserved;
            ++reserved_count_;
            assert_invariants_locked();
            return ReservedSlot{this, index};
        }

        [[nodiscard]] std::optional<ReservedSlot> try_take_quarantined() {
            std::lock_guard lock{mutex_};
            if (closed_.load(std::memory_order_relaxed) || quarantined_.empty()) {
                return std::nullopt;
            }
            const auto index = quarantined_.back();
            quarantined_.pop_back();
            assert(states_[index] == SlotState::quarantined);
            states_[index] = SlotState::reserved;
            ++reserved_count_;
            assert_invariants_locked();
            return ReservedSlot{this, index, true};
        }

        [[nodiscard]] std::optional<ReservedSlot> try_take_idle() {
            if (closed_.load(std::memory_order_acquire) || waiter_gate_.load(std::memory_order_seq_cst) != 0) {
                return std::nullopt;
            }
            const auto index = claim_idle();
            if (!index) {
                return std::nullopt;
            }
            std::unique_lock lock{mutex_};
            if (closed_.load(std::memory_order_relaxed)) {
                quarantine_borrowed(*index);
                return std::nullopt;
            }
            if (waiter_gate_.load(std::memory_order_seq_cst) != 0) {
                states_[*index] = SlotState::idle;
                publish_bit(*index);
                // Registration may have rescanned while maintenance held this
                // bit; publication must now serve that registered demand.
                serve_waiters_locked(lock);
                return std::nullopt;
            }
            states_[*index] = SlotState::reserved;
            --usable_size_;
            ++reserved_count_;
            assert_invariants_locked();
            return ReservedSlot{this, *index, true};
        }

        [[nodiscard]] std::size_t capacity() const noexcept {
            return states_.size();
        }

        [[nodiscard]] std::size_t size() const noexcept {
            std::lock_guard lock{mutex_};
            return usable_size_;
        }

        [[nodiscard]] std::size_t quarantined_count() const noexcept {
            std::lock_guard lock{mutex_};
            return quarantined_.size();
        }

        [[nodiscard]] std::size_t waiter_count() const noexcept {
            return waiter_gate_.load(std::memory_order_seq_cst);
        }

        [[nodiscard]] PoolStats stats() const noexcept {
            std::lock_guard lock{mutex_};
            // Exact inventory fields are fixed by mutex; the idle scan can observe
            // concurrent bit claims. Idle/borrowed are bounded telemetry whose
            // sum remains exactly usable_size, not an instantaneous traffic view.
            const std::size_t idle = std::min(idle_count(), usable_size_);
            return {
                .capacity    = states_.size(),
                .vacant      = vacant_.size(),
                .reserved    = reserved_count_,
                .idle        = idle,
                .borrowed    = usable_size_ - idle,
                .quarantined = quarantined_.size(),
                .waiters     = waiters_.size(),
            };
        }

        [[nodiscard]] bool is_shutdown() const noexcept {
            return closed_.load(std::memory_order_acquire);
        }

        void shutdown() noexcept {
            std::unique_lock lock{mutex_};
            if (closed_.exchange(true, std::memory_order_seq_cst)) {
                return;
            }
            for (std::size_t w = 0; w < words_.size(); ++w) {
                std::uint64_t bits = words_[w].bits.exchange(0, std::memory_order_seq_cst);
                while (bits != 0) {
                    const std::size_t index  = w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bits));
                    bits                    &= bits - 1;
                    assert(states_[index] == SlotState::idle);
                    states_[index] = SlotState::quarantined;
                    quarantined_.push_back(index);
                    --usable_size_;
                }
            }
            while (!waiters_.empty()) {
                auto& waiter     = waiters_.front();
                const auto state = waiter.bounded() && std::chrono::steady_clock::now() >= waiter.deadline
                                       ? detail::pool_waiters::WaiterState::timeout
                                       : detail::pool_waiters::WaiterState::shutdown;
                complete_waiter_locked(waiter, state, lock);
                lock.lock();
            }
            assert_invariants_locked();
        }

    private:
        friend class detail::PoolBorrowed<T>;
        friend class detail::PoolReservedSlot<T>;
        friend class detail::PoolSyncAcquire<T>;
        friend class detail::PoolAsyncAcquire<T>;
        friend class detail::AsyncPoolReference<Pool<T>>;

        enum class SlotState : std::uint8_t { vacant, reserved, idle, borrowed, quarantined };

        // No bookkeeping in the arena stride, and no implicit T lifetime.
        union Slot {
            constexpr Slot() noexcept {
            }
            ~Slot() noexcept {
            }
            T value;
        };

        struct alignas(std::hardware_destructive_interference_size) PaddedWord {
            std::atomic<std::uint64_t> bits{0};
        };
        static constexpr std::size_t bits_per_word = 64;

        [[nodiscard]] T* slot_ptr(const std::size_t index) noexcept {
            return std::addressof(storage_[index].value);
        }

        // Caller holds mutex. Never scan plain states: fast bit owners may
        // modify their own entries concurrently with this cold bookkeeping.
        void assert_invariants_locked() const noexcept {
            assert(states_.size() == vacant_.size() + reserved_count_ + quarantined_.size() + usable_size_);
            assert(waiter_gate_.load(std::memory_order_relaxed) == waiters_.size());
            assert(idle_count() <= usable_size_);
            beaver::force_non_static(this);
        }

        void publish_bit(const std::size_t index) noexcept {
            words_[index / bits_per_word].bits.fetch_or(std::uint64_t{1} << (index % bits_per_word),
                                                       std::memory_order_seq_cst);
        }

        [[nodiscard]] std::optional<std::size_t> claim_idle(const bool strong = false) noexcept {
            // Both the initial load and CAS failure can observe shutdown's
            // release bitmap sweep. Acquire observes its preceding closure
            // before the caller rechecks closed, including failure-to-zero.
            // The strong scan also participates in the waiter-gate SC order.
            const auto order = strong ? std::memory_order_seq_cst : std::memory_order_acquire;
            for (std::size_t w = 0; w < words_.size(); ++w) {
                auto& word = words_[w].bits;
                auto bits  = word.load(order);
                while (bits != 0) {
                    const auto index = w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bits));
                    if (word.compare_exchange_weak(bits, bits & (bits - 1), order, order)) {
                        // The bit transfers exclusive ownership of this state
                        // as well as the resource; no other path may inspect
                        // the entry without owning its bit/token.
                        assert(states_[index] == SlotState::idle);
                        states_[index] = SlotState::borrowed;
                        return index;
                    }
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] bool reclaim_idle(const std::size_t index) noexcept {
            auto& word     = words_[index / bits_per_word].bits;
            const auto bit = std::uint64_t{1} << (index % bits_per_word);
            auto bits      = word.load(std::memory_order_relaxed);
            while ((bits & bit) != 0) {
                if (word.compare_exchange_weak(
                        bits, bits & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                    assert(states_[index] == SlotState::idle);
                    states_[index] = SlotState::borrowed;
                    return true;
                }
            }
            return false;
        }

        // Caller owns this slot and holds mutex. Registries are preallocated.
        void quarantine_borrowed(const std::size_t index) noexcept {
            assert(states_[index] == SlotState::borrowed);
            states_[index] = SlotState::quarantined;
            --usable_size_;
            quarantined_.push_back(index);
            assert_invariants_locked();
        }

        // Exact removal under mutex. The stack owner uses this directly on
        // self-timeout; other completions additionally pin its notification.
        void unlink_waiter(detail::pool_waiters::WaiterNode& waiter,
                           const detail::pool_waiters::WaiterState state) noexcept {
            assert(waiter.state == detail::pool_waiters::WaiterState::queued);
            waiters_.erase(waiters_.iterator_to(waiter));
            waiter_gate_.fetch_sub(1, std::memory_order_seq_cst);
            waiter.state = state;
            assert_invariants_locked();
        }

        // Consumes the pool lock. Every terminal selector uses this same
        // unlink/dispatch path for both stack and heap waiter payloads.
        void complete_waiter_locked(detail::pool_waiters::WaiterNode& waiter,
                                    const detail::pool_waiters::WaiterState state,
                                    std::unique_lock<std::mutex>& lock) noexcept {
            unlink_waiter(waiter, state);
            switch (waiter.kind) {
                case detail::pool_waiters::WaiterKind::sync: {
                    // The concrete payload initializes the kind tag; a vtable is unnecessary.
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                    auto& sync = static_cast<detail::pool_waiters::SyncWaiter&>(waiter);
                    const std::lock_guard notification{sync.notification_mutex};
                    lock.unlock();
                    sync.cv.notify_one();
                    return;
                }
                case detail::pool_waiters::WaiterKind::async: {
                    // Adopt the queue's reference under mutex, then transfer
                    // it to the executor completion after releasing mutex.
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                    auto* payload = static_cast<detail::pool_waiters::AsyncWaiterNode*>(&waiter);
                    boost::intrusive_ptr<detail::pool_waiters::AsyncWaiterNode> async{payload, false};
                    lock.unlock();
                    payload->complete(std::move(async));
                    return;
                }
            }
            std::unreachable();
        }

        // Caller exclusively owns a usable slot. A selected waiter consumes
        // the lock; quarantine/publication leave it owned by the caller.
        void handoff_or_publish_locked(const std::size_t index, std::unique_lock<std::mutex>& lock) noexcept {
            for (;;) {
                if (closed_.load(std::memory_order_relaxed)) {
                    quarantine_borrowed(index);
                    return;
                }
                if (waiters_.empty()) {
                    states_[index] = SlotState::idle;
                    publish_bit(index);
                    return;
                }
                auto& waiter = waiters_.front();
                if (waiter.bounded() && std::chrono::steady_clock::now() >= waiter.deadline) {
                    complete_waiter_locked(waiter, detail::pool_waiters::WaiterState::timeout, lock);
                    lock.lock();
                    continue;
                }
                assert(states_[index] == SlotState::borrowed);
                waiter.assigned_slot = index;
                complete_waiter_locked(waiter, detail::pool_waiters::WaiterState::assigned, lock);
                return;
            }
        }

        void serve_waiters_locked(std::unique_lock<std::mutex>& lock) noexcept {
            while (!closed_.load(std::memory_order_relaxed) && !waiters_.empty()) {
                const auto index = claim_idle(true);
                if (!index) {
                    return;
                }
                handoff_or_publish_locked(*index, lock);
                if (!lock.owns_lock()) {
                    lock.lock();
                }
            }
        }

        void handoff_or_publish(const std::size_t index) noexcept {
            if (closed_.load(std::memory_order_acquire) || waiter_gate_.load(std::memory_order_seq_cst) != 0) {
                std::unique_lock lock{mutex_};
                handoff_or_publish_locked(index, lock);
                return;
            }
            assert(states_[index] == SlotState::borrowed);
            states_[index] = SlotState::idle;
            publish_bit(index);
            // Closure and its sweep are seq_cst too: either the sweep sees
            // our publication or this check sees closure and reclaims it.
            // Only the winner of the bit may touch its plain slot state.
            if (closed_.load(std::memory_order_seq_cst)) {
                if (reclaim_idle(index)) {
                    std::lock_guard lock{mutex_};
                    quarantine_borrowed(index);
                }
                return;
            }
            if (waiter_gate_.load(std::memory_order_seq_cst) != 0) {
                std::unique_lock lock{mutex_};
                serve_waiters_locked(lock);
            }
        }

        [[nodiscard]] std::size_t idle_count() const noexcept {
            std::size_t count = 0;
            for (const auto& word : words_) {
                count += static_cast<std::size_t>(std::popcount(word.bits.load(std::memory_order_acquire)));
            }
            return count;
        }

        std::unique_ptr<Slot[]> storage_;
        std::vector<SlotState> states_;
        std::vector<std::size_t> vacant_;
        std::vector<std::size_t> quarantined_;
        std::vector<PaddedWord> words_;
        mutable std::mutex mutex_;
        detail::pool_waiters::WaiterList waiters_;
        // A parker increments this gate, then strongly scans the idle words,
        // all seq_cst. A returning borrower publishes its bit, then rechecks
        // the gate, also seq_cst. Their total order forbids both sides from
        // missing each other. If the parker missed the bit, the publisher
        // takes mutex and drains available bits to the registered FIFO.
        std::atomic<std::size_t> waiter_gate_{0};
        std::size_t usable_size_    = 0;
        std::size_t reserved_count_ = 0;
        std::atomic<bool> closed_{false};
        [[no_unique_address]] detail::AsyncOperationCount async_operations_;
    };

}  // namespace menagerie::starling
