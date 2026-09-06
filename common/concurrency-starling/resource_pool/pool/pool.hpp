#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <menagerie/beaver>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

namespace menagerie::starling {

    /// Which waiting machinery a Pool instantiation carries: `sync` parks the calling
    /// thread on a condition variable; `async` suspends an asio operation. The verbs
    /// share names across modes; `requires` rejects the wrong-mode call.
    enum class Wait : std::uint8_t { sync, async };

    /**
     * @brief A runtime-sized pool of `T` resources with lazy creation and direct
     *        FIFO handoff — the unified successor of ResourcePool/AsyncResourcePool
     *        and elephant's ConnectionPool engine.
     *
     * `capacity` is the admission-control cap (never a template parameter: it comes
     * from config, and grow/shrink policies need it mutable at runtime later);
     * `min_eager` resources are created in the constructor, the rest on demand.
     * A `Handle` returns its resource on destruction — to the head waiter directly
     * when someone is parked (direct handoff; cannot be barged), else by publishing
     * an idle bit that any `try_acquire` may claim. `mark_dead()` makes the release
     * destroy the resource instead, freeing capacity; when waiters are parked, the
     * pool then kicks the queue so a replacement is created on a waiter's thread
     * (the release path itself never runs the factory).
     *
     * The factory reports failure by returning `std::nullopt`; it must not throw
     * (release-path recreation would have nowhere to propagate to). Outstanding
     * `Handle`s must not outlive the pool; `shutdown()` (idempotent, run by the
     * destructor) drains waiters and destroys idle resources.
     *
     * @note The pool must also outlive any completion still queued on a
     *       caller-supplied executor: parked completions are posted carrying a
     *       Handle by value, and a queued-but-never-run completion destroys that
     *       Handle (and touches the pool) at executor teardown. Drain or stop the
     *       executors before destroying the pool.
     */
    template <Wait Mode, typename T>
    class Pool : beaver::Immutable {
        // Factory results are moved into arena slots on paths with no rollback for a
        // throwing move (lazy creation past the optimistic capacity reservation, eager
        // fill mid-constructor), and slots are destroyed on noexcept release paths.
        static_assert(std::is_nothrow_move_constructible_v<T>, "Pool<T>: T's move constructor must be noexcept");
        static_assert(std::is_nothrow_destructible_v<T>, "Pool<T>: T's destructor must be noexcept");

        /// Uninitialized inline storage for one T. Live slots are placement-constructed
        /// (construct_at) and destroyed (destroy_at) explicitly; liveness is tracked
        /// entirely by the external bookkeeping (bits / spare_slot_ids_ / created_).
        union Slot {
            Slot() noexcept {
            }
            ~Slot() noexcept {
            }
            T value;
        };

        static constexpr std::size_t NPOS = static_cast<std::size_t>(-1);

    public:
        using value_type = T;
        using Factory    = std::function<std::optional<T>()>;

        /// Move-only lease over one pooled resource. Destruction releases the slot;
        /// `mark_dead()` makes that release drop the resource instead of recycling it.
        class Handle : beaver::NonCopyable {
        public:
            Handle() noexcept = default;
            Handle(Handle&& o) noexcept
                : pool_{o.pool_},
                  ptr_{o.ptr_},
                  slot_{o.slot_},
                  dead_{o.dead_} {
                o.ptr_ = nullptr;
            }
            Handle& operator=(Handle&& o) noexcept {
                if (this != &o) {
                    release_now();
                    pool_  = o.pool_;
                    ptr_   = o.ptr_;
                    slot_  = o.slot_;
                    dead_  = o.dead_;
                    o.ptr_ = nullptr;
                }
                return *this;
            }
            ~Handle() noexcept {
                release_now();
            }

            [[nodiscard]] T& operator*() const noexcept {
                assert(ptr_ != nullptr && "dereferencing an empty Pool::Handle");
                return *ptr_;
            }
            [[nodiscard]] T* operator->() const noexcept {
                assert(ptr_ != nullptr && "dereferencing an empty Pool::Handle");
                return ptr_;
            }
            [[nodiscard]] T* get() const noexcept {
                return ptr_;
            }
            [[nodiscard]] explicit operator bool() const noexcept {
                return ptr_ != nullptr;
            }
            /// The resource is broken: release will destroy it (freeing capacity)
            /// instead of returning it to circulation.
            void mark_dead() noexcept {
                dead_ = true;
            }

        private:
            friend class Pool;
            Handle(Pool* p, T* ptr, const std::size_t slot) noexcept
                : pool_{p},
                  ptr_{ptr},
                  slot_{slot} {
            }
            void release_now() noexcept {
                if (ptr_ != nullptr) {
                    pool_->release(slot_, dead_);
                    ptr_  = nullptr;
                    dead_ = false;
                }
            }
            Pool* pool_       = nullptr;
            T* ptr_           = nullptr;
            std::size_t slot_ = 0;
            bool dead_        = false;
        };

        /**
         * @brief Build a pool capped at @p capacity, eagerly creating @p min_eager
         *        resources (creation failures during eager fill are skipped — the
         *        pool starts smaller and grows lazily).
         * @throw std::invalid_argument if capacity == 0, min_eager > capacity, or
         *        the factory is empty.
         */
        Pool(const std::size_t capacity, const std::size_t min_eager, Factory make)
            : capacity_{capacity},
              make_{std::move(make)} {
            if (capacity == 0) {
                throw std::invalid_argument{"Pool: capacity must be non-zero"};
            }
            if (min_eager > capacity) {
                throw std::invalid_argument{"Pool: min_eager exceeds capacity"};
            }
            if (!make_) {
                throw std::invalid_argument{"Pool: factory must be callable"};
            }
            storage_ = std::make_unique<Slot[]>(capacity);
            words_   = std::vector<PaddedWord>((capacity + bits_per_word - 1) / bits_per_word);
            spare_slot_ids_.reserve(capacity);
            for (std::size_t i = capacity; i > 0; --i) {
                spare_slot_ids_.push_back(i - 1);  // pop_back yields 0, 1, 2, ...
            }
            for (std::size_t i = 0; i < min_eager; ++i) {
                if (std::optional<T> v = make_()) {
                    const std::size_t slot = spare_slot_ids_.back();
                    spare_slot_ids_.pop_back();
                    std::construct_at(slot_ptr(slot), std::move(*v));
                    ++created_;
                    publish_bit(slot);
                }
            }
        }

        ~Pool() noexcept {
            shutdown();
            assert(created_ == 0 && "Pool destroyed with outstanding Handles");
        }

        /// Non-waiting acquire: lock-free claim of an idle node, else lazy create up
        /// to capacity, else nullopt. A call racing a concurrent `shutdown()` may
        /// still return a live Handle (a stale-gate-published bit claimed just before
        /// the shutdown sweep); that Handle destroys its resource on release as usual.
        [[nodiscard]] std::optional<Handle> try_acquire() {
            if (shutdown_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            if (const std::size_t s = try_claim(); s != NPOS) {
                return Handle{this, slot_ptr(s), s};
            }
            std::unique_lock lk{mtx_};
            if (shutdown_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            if (const std::size_t s = obtain_locked(lk); s != NPOS) {
                return Handle{this, slot_ptr(s), s};
            }
            return std::nullopt;
        }

        /// Block the calling thread until a resource frees (or shutdown). Wait::sync only.
        [[nodiscard]] std::optional<Handle> acquire()
            requires(Mode == Wait::sync)
        {
            return sync_acquire(std::nullopt);
        }

        /// Block up to @p timeout. Nullopt on timeout or shutdown. Wait::sync only.
        [[nodiscard]] std::optional<Handle> acquire_for(const std::chrono::steady_clock::duration timeout)
            requires(Mode == Wait::sync)
        {
            return sync_acquire(timeout);
        }

        /// Suspend the awaiting coroutine until a resource frees; `co_await` yields
        /// `std::optional<Handle>` — engaged on success, nullopt on shutdown or
        /// cancellation — mirroring the sync verb exactly. Coroutine-only by design
        /// (no completion-token parameter): completions post to the awaiting
        /// coroutine's executor, which also runs the wait timer and creation kicks.
        /// For an inline fast path in tight loops call `try_acquire()` first — a
        /// non-suspending `co_return` resumes the caller via symmetric transfer at
        /// constant stack; the awaited verbs always post. Wait::async only.
        [[nodiscard]] boost::asio::awaitable<std::optional<Handle>> acquire()
            requires(Mode == Wait::async)
        {
            boost::asio::use_awaitable_t<> token;
            return boost::asio::async_initiate<boost::asio::use_awaitable_t<>, void(std::optional<Handle>)>(
                [this]<typename Handler>(Handler&& h) {
                    this->async_initiate_acquire(std::nullopt, std::forward<Handler>(h));
                },
                token);
        }

        /// Bounded variant: additionally yields nullopt if @p timeout elapses first.
        /// Wait::async only.
        [[nodiscard]] boost::asio::awaitable<std::optional<Handle>>
        acquire_for(const std::chrono::steady_clock::duration timeout)
            requires(Mode == Wait::async)
        {
            boost::asio::use_awaitable_t<> token;
            return boost::asio::async_initiate<boost::asio::use_awaitable_t<>, void(std::optional<Handle>)>(
                [this, timeout]<typename Handler>(Handler&& h) {
                    this->async_initiate_acquire(timeout, std::forward<Handler>(h));
                },
                token);
        }

        /// Drain every parked waiter and destroy all idle resources. Checked-out
        /// resources die when their Handle releases. Idempotent.
        void shutdown() {
            if (shutdown_.exchange(true, std::memory_order_acq_rel)) {
                return;
            }
            std::unique_lock lk{mtx_};
            drain_waiters_locked(lk);
            for (std::size_t w = 0; w < words_.size(); ++w) {
                std::uint64_t v = words_[w].bits.exchange(0, std::memory_order_seq_cst);
                while (v != 0) {
                    const std::uint64_t bit  = v & (~v + 1);
                    v                       &= ~bit;
                    const std::size_t slot   = w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bit));
                    destroy_locked(slot);
                }
            }
        }

        [[nodiscard]] std::size_t capacity() const noexcept {
            return capacity_;
        }
        /// Sum of set bits across `words_`. Exact only when quiescent (no in-flight
        /// release/acquire): under concurrent traffic a node may be mid-transition
        /// (claimed but not yet counted anywhere else), so this is a snapshot.
        [[nodiscard]] std::size_t free_count() const noexcept {
            const std::lock_guard lk{mtx_};
            std::size_t n = 0;
            for (const auto& w : words_) {
                n += static_cast<std::size_t>(std::popcount(w.bits.load(std::memory_order_relaxed)));
            }
            return n;
        }
        /// `created_` minus the idle-bit popcount. Exact only when quiescent (see
        /// `free_count`).
        [[nodiscard]] std::size_t active_count() const noexcept {
            const std::lock_guard lk{mtx_};
            std::size_t free_n = 0;
            for (const auto& w : words_) {
                free_n += static_cast<std::size_t>(std::popcount(w.bits.load(std::memory_order_relaxed)));
            }
            return created_ - free_n;
        }
        [[nodiscard]] std::size_t waiter_count() const noexcept {
            const std::lock_guard lk{mtx_};
            assert(n_waiters_.load(std::memory_order_relaxed) == waiters_.size() &&
                   "n_waiters_ gate out of sync with the waiter deque");
            return waiters_.size();
        }
        [[nodiscard]] bool is_shutdown() const noexcept {
            return shutdown_.load(std::memory_order_acquire);
        }

    private:
        struct SyncWaiter {
            std::size_t assigned = NPOS;  ///< Handed-off slot; npos until ready (or on shutdown).
            std::condition_variable cv;   ///< Signaled by handoff, capacity kick, or shutdown.
            bool ready  = false;          ///< True once assigned is valid or the wait was broken.
            bool kicked = false;          ///< Woken to retry creation on freed capacity (see kick_locked).
        };
        struct AsyncWaiter : beaver::NonCopyable {
            AsyncWaiter(Pool* p, boost::asio::any_io_executor e)
                : pool{p},
                  exec{std::move(e)},
                  timer{exec} {
            }
            Pool* pool;  ///< Reached by slot/timer callbacks only via a live waiter.
            /// The awaiting coroutine's executor: completions post here; it also runs
            /// the wait timer and creation kicks.
            boost::asio::any_io_executor exec;
            boost::asio::steady_timer timer;  ///< Armed only for bounded waits.
            boost::asio::any_completion_handler<void(std::optional<Handle>)>
                handler{};                    ///< Moved out exactly once by the claim winner.
            std::atomic_bool claimed{false};  ///< First of {handoff, timeout, cancel, shutdown} wins.
        };
        using WaiterT = std::conditional_t<Mode == Wait::async, std::shared_ptr<AsyncWaiter>, SyncWaiter*>;

        /// Per-thread start-word hint spreading CAS traffic (same trick as the old pools).
        [[nodiscard]] static std::size_t word_hint() noexcept {
            const thread_local std::size_t hint = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return hint;
        }

        void publish_bit(const std::size_t slot) noexcept {
            words_[slot / bits_per_word].bits.fetch_or(std::uint64_t{1} << (slot % bits_per_word),
                                                       std::memory_order_seq_cst);
        }

        /// Try to clear our own slot's bit (post-publish shutdown reclaim).
        [[nodiscard]] bool try_claim_bit(const std::size_t slot) noexcept {
            std::atomic<std::uint64_t>& word = words_[slot / bits_per_word].bits;
            const std::uint64_t bit          = std::uint64_t{1} << (slot % bits_per_word);
            std::uint64_t cur                = word.load(std::memory_order_relaxed);
            while ((cur & bit) != 0) {
                if (word.compare_exchange_weak(cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

        /// Address of the (assumed live) resource in `slot`. Valid to call lock-free
        /// once the caller has won that slot's bit (or holds the slot outright).
        [[nodiscard]] T* slot_ptr(const std::size_t slot) const noexcept {
            return &storage_[slot].value;
        }

        /// Lock-free claim of any idle slot. `strong` = seq_cst loads/CAS, required for
        /// the parker's post-increment re-scan (see the gate invariant comment).
        [[nodiscard]] std::size_t try_claim(const bool strong = false) noexcept {
            const std::memory_order load_order = strong ? std::memory_order_seq_cst : std::memory_order_relaxed;
            const std::memory_order ok_order   = strong ? std::memory_order_seq_cst : std::memory_order_acquire;
            const std::size_t n_words          = words_.size();
            if (n_words == 0) {
                return NPOS;
            }
            const std::size_t start = word_hint() % n_words;
            for (std::size_t k = 0; k < n_words; ++k) {
                std::atomic<std::uint64_t>& word = words_[(start + k) % n_words].bits;
                std::uint64_t cur                = word.load(load_order);
                while (cur != 0) {
                    if (const std::uint64_t bit = cur & (~cur + 1);
                        word.compare_exchange_weak(cur, cur & ~bit, ok_order, std::memory_order_relaxed)) {
                        return ((start + k) % n_words) * bits_per_word +
                               static_cast<std::size_t>(std::countr_zero(bit));
                    }
                }
            }
            return NPOS;
        }

        /// Claim an idle slot via the bitset fast path (already tried by the caller
        /// before locking; retried here in case a release published one while we
        /// waited for the lock), else lazy create (factory runs OUTSIDE the mutex;
        /// `created_` is incremented optimistically and the slot id reserved, both
        /// rolled back on failure so concurrent creators cannot overshoot capacity).
        /// Returns `npos` on exhaustion/failure.
        [[nodiscard]] std::size_t obtain_locked(std::unique_lock<std::mutex>& lk, const bool kick_on_fail = true) {
            if (const std::size_t s = try_claim(); s != NPOS) {
                return s;
            }
            if (created_ >= capacity_) {
                return NPOS;
            }
            ++created_;  // optimistic; keeps concurrent creators under the cap
            const std::size_t slot = spare_slot_ids_.back();
            spare_slot_ids_.pop_back();
            lk.unlock();
            std::optional<T> v = make_();
            lk.lock();
            if (!v || shutdown_.load(std::memory_order_acquire)) {
                --created_;
                spare_slot_ids_.push_back(slot);
                // The optimistic increment may have turned away acquirers that then
                // parked; hand one of them the freed budget. One-shot: a kicked retry
                // passes kick_on_fail=false so a broken factory cannot cascade.
                if (kick_on_fail && !shutdown_.load(std::memory_order_acquire)) {
                    kick_locked();
                }
                return NPOS;
            }
            std::construct_at(slot_ptr(slot), std::move(*v));
            return slot;  // handed to the caller; its bit stays clear
        }

        /// A capacity-freeing event (dead release, creation rollback) cannot run the
        /// factory itself — release must never block on resource creation — so it
        /// kicks the queue instead: sync mode wakes the head waiter to retry creation
        /// on its own thread; async mode posts a one-shot creator job to the head
        /// waiter's executor. One kick per event, so a broken factory yields one
        /// failed retry and re-parks the queue until the next event — no retry storm.
        void kick_locked() {
            if (waiters_.empty()) {
                return;
            }
            if constexpr (Mode == Wait::sync) {
                SyncWaiter* w = waiters_.front();
                waiters_.pop_front();
                n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                w->kicked = true;
                w->ready  = true;
                w->cv.notify_one();  // under the lock: the waiter's frame owns *w
            } else {
                boost::asio::post(waiters_.front()->exec, [this] { this->async_run_kick(); });
            }
        }

        /// One-shot creator job (see kick_locked): builds a resource on a waiter's
        /// executor thread — the same thread class that runs the factory on the
        /// acquire path — and hands it to the queue head, else banks it as an idle bit.
        void async_run_kick()
            requires(Mode == Wait::async)
        {
            std::unique_lock lk{mtx_};
            if (shutdown_.load(std::memory_order_acquire) || waiters_.empty()) {
                return;
            }
            const std::size_t s = obtain_locked(lk, /*kick_on_fail=*/false);
            if (s == NPOS) {
                return;  // capacity re-claimed by others or factory failed; next event retries
            }
            if (!hand_off_locked(s, lk)) {
                // Everyone left (timeout/cancel) during the factory window: bank the
                // resource. Published while mtx_ is held, so a concurrent shutdown's
                // sweep (which takes mtx_ after setting the flag) cannot miss the bit.
                publish_bit(s);
            }
        }

        [[nodiscard]] std::optional<Handle>
        sync_acquire(const std::optional<std::chrono::steady_clock::duration> timeout)
            requires(Mode == Wait::sync)
        {
            if (shutdown_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            if (const std::size_t s = try_claim(); s != NPOS) {
                return Handle{this, slot_ptr(s), s};
            }
            std::optional<std::chrono::steady_clock::time_point> deadline;
            if (timeout) {
                deadline = std::chrono::steady_clock::now() + *timeout;
            }
            std::unique_lock lk{mtx_};
            bool kicked = false;  // holding an unspent capacity kick (see kick_locked)
            for (;;) {
                if (shutdown_.load(std::memory_order_acquire)) {
                    return std::nullopt;  // drain wakes everyone; a held kick needs no forwarding
                }
                if (deadline && std::chrono::steady_clock::now() >= *deadline) {
                    if (kicked) {
                        kick_locked();  // pass the unspent kick to the next waiter
                    }
                    return std::nullopt;
                }
                // First pass: the ordinary claim-or-create attempt. Kicked passes: the
                // retry the kick paid for — its own failure must not cascade a new kick.
                if (const std::size_t s = obtain_locked(lk, /*kick_on_fail=*/!kicked); s != NPOS) {
                    return Handle{this, slot_ptr(s), s};
                }
                kicked = false;
                n_waiters_.fetch_add(1, std::memory_order_seq_cst);
                if (const std::size_t s = try_claim(true); s != NPOS) {  // strong re-scan AFTER count publication
                    n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                    return Handle{this, slot_ptr(s), s};
                }
                SyncWaiter w;
                waiters_.push_back(&w);
                const auto pred = [&] { return w.ready || shutdown_.load(std::memory_order_acquire); };
                if (deadline) {
                    w.cv.wait_until(lk, *deadline, pred);
                } else {
                    w.cv.wait(lk, pred);
                }
                if (w.ready && w.assigned != NPOS) {
                    return Handle{this, slot_ptr(w.assigned), w.assigned};
                }
                if (w.ready && w.kicked) {
                    kicked = true;  // kick_locked already unlinked us; retry creation from the top
                    continue;
                }
                // Timeout or shutdown: unregister if a handoff has not already claimed us.
                if (std::erase(waiters_, &w) != 0) {
                    n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                }
                return std::nullopt;
            }
        }

        template <typename Handler>
        void async_initiate_acquire(const std::optional<std::chrono::steady_clock::duration> timeout, Handler&& handler)
            requires(Mode == Wait::async)
        {
            // The awaitable handler always carries the awaiting coroutine's executor;
            // every completion — fast path and parked alike — is POSTED to it. Never
            // dispatch inline: a resume under a release() call stack would recurse
            // unboundedly in tight acquire/release loops (the try_acquire-first idiom
            // is how callers get an inline fast path).
            auto cex                = boost::asio::any_io_executor{boost::asio::get_associated_executor(handler)};
            const auto complete_now = [&](std::optional<Handle> hd) {
                boost::asio::post(cex, [h = std::forward<Handler>(handler), hd = std::move(hd)]() mutable {
                    std::move(h)(std::move(hd));
                });
            };
            if (shutdown_.load(std::memory_order_acquire)) {
                complete_now(std::nullopt);
                return;
            }
            if (const std::size_t s = try_claim(); s != NPOS) {
                complete_now(Handle{this, slot_ptr(s), s});
                return;
            }
            std::unique_lock lk{mtx_};
            if (shutdown_.load(std::memory_order_acquire)) {
                lk.unlock();
                complete_now(std::nullopt);
                return;
            }
            if (const std::size_t s = obtain_locked(lk); s != NPOS) {
                lk.unlock();
                complete_now(Handle{this, slot_ptr(s), s});
                return;
            }
            // Recheck shutdown: if it completed during obtain_locked's factory unlock window,
            // we must not park (post-shutdown release never hands off, unbounded waiters have
            // no timer to expire them). Hold the lock from here through waiters_.push_back.
            if (shutdown_.load(std::memory_order_acquire)) {
                lk.unlock();
                complete_now(std::nullopt);
                return;
            }
            n_waiters_.fetch_add(1, std::memory_order_seq_cst);
            if (const std::size_t s = try_claim(true); s != NPOS) {
                n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                lk.unlock();
                complete_now(Handle{this, slot_ptr(s), s});
                return;
            }
            // Park. Read the cancellation slot from the CONCRETE handler BEFORE moving
            // it into the waiter (after publication a concurrent completion may move
            // the handler out from under us — the old async pool's hard-won lesson).
            auto slot       = boost::asio::get_associated_cancellation_slot(handler);
            auto waiter     = std::make_shared<AsyncWaiter>(this, std::move(cex));
            waiter->handler = std::forward<Handler>(handler);
            if (slot.is_connected()) {
                // Deliberate order: constructing the any_completion_handler above emplaced
                // asio's cancellation-forwarding proxy into this same slot; assigning here
                // replaces it, routing cancellation to the pool (which completes the
                // handler itself). The callback stays installed in the caller-owned slot
                // after the operation completes — it must dereference nothing until the
                // weak_ptr proves the waiter (and therefore the pool) is still alive.
                slot.assign([wp = std::weak_ptr<AsyncWaiter>{waiter}](boost::asio::cancellation_type) {
                    if (const auto w = wp.lock()) {
                        w->pool->async_abort(w);
                    }
                });
            }
            waiters_.push_back(waiter);
            if (timeout) {
                waiter->timer.expires_after(*timeout);
                waiter->timer.async_wait(
                    [wp = std::weak_ptr<AsyncWaiter>{waiter}](const boost::system::error_code& ec) {
                        if (ec == boost::asio::error::operation_aborted) {
                            return;  // cancelled by a successful handoff/shutdown
                        }
                        if (const auto w = wp.lock()) {
                            w->pool->async_abort(w);
                        }
                    });
            }
        }

        /// Timeout / cancellation arm: first claim wins; erase + complete nullopt.
        void async_abort(const std::shared_ptr<AsyncWaiter>& w) noexcept
            requires(Mode == Wait::async)
        {
            std::unique_lock lk{mtx_};
            if (bool expected = false; !w->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                return;  // handoff or shutdown already owns this waiter
            }
            if (std::erase_if(waiters_, [&](const WaiterT& e) { return e.get() == w.get(); }) != 0) {
                n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
            }
            boost::asio::any_completion_handler<void(std::optional<Handle>)> handler = std::move(w->handler);
            const boost::asio::any_io_executor exec                                  = std::move(w->exec);
            lk.unlock();
            boost::asio::post(exec, [h = std::move(handler)]() mutable { std::move(h)(std::nullopt); });
        }

        /// Destroy the resource in `slot` and free the slot index for reuse.
        void destroy_locked(const std::size_t slot) noexcept {
            assert(slot < capacity_);
            std::destroy_at(slot_ptr(slot));
            spare_slot_ids_.push_back(slot);
            --created_;
        }

        /// Handle release path. Never runs the factory (a Handle destructor must not
        /// block on resource creation) — a dead release frees capacity and kicks the
        /// waiter queue so recreation happens on a waiter's thread instead. See the
        /// idle-tracking invariant comment near the member declarations for the full
        /// release-side gate protocol.
        void release(const std::size_t slot, const bool dead) noexcept {
            if (dead || shutdown_.load(std::memory_order_acquire)) {
                const std::lock_guard lk{mtx_};
                destroy_locked(slot);
                if (!shutdown_.load(std::memory_order_acquire)) {
                    kick_locked();  // freed capacity can serve a parked waiter via recreation
                }
                return;
            }
            // Fair handoff: if someone is (probably) parked, hand the node to the FIFO
            // head under the mutex — direct handoff never publishes the bit, so it
            // cannot be barged.
            if (n_waiters_.load(std::memory_order_seq_cst) != 0) {
                std::unique_lock lk{mtx_};
                if (shutdown_.load(std::memory_order_acquire)) {
                    destroy_locked(slot);
                    return;
                }
                if (hand_off_locked(slot, lk)) {
                    return;  // lock released inside on success
                }
            }  // stale gate (or all waiters already claimed): fall through and publish.
               // Once published below, a drive-by try_acquire may claim the bit before
               // a parked waiter is served (rare window; no wakeup is lost — the parker
               // simply re-scans and stays parked).
            publish_bit(slot);
            // A racing shutdown sweep may have run before our publish: reclaim our own
            // bit and destroy, exactly once (seq_cst orders publish before this load).
            if (shutdown_.load(std::memory_order_seq_cst)) {
                if (try_claim_bit(slot)) {
                    const std::lock_guard lk{mtx_};
                    destroy_locked(slot);
                }
                return;
            }
            // Dekker second check: a parker that missed our bit must be visible here.
            if (n_waiters_.load(std::memory_order_seq_cst) != 0) {
                std::unique_lock lk{mtx_};
                serve_waiters_locked(lk);
            }
        }

        /// Claim idle bits and hand them to parked waiters, FIFO, while both exist.
        /// Entered and left with the lock held OR released — the unique_lock knows.
        void serve_waiters_locked(std::unique_lock<std::mutex>& lk) noexcept {
            while (!waiters_.empty()) {
                const std::size_t m = try_claim();
                if (m == NPOS) {
                    return;  // bits stolen by fast-path claimers; waiters stay parked
                }
                if (!hand_off_locked(m, lk)) {
                    // Defensive: unreachable today — claimants unlink in the same
                    // critical section, so `waiters_` cannot be all-claimed while
                    // non-empty here. If ever reached, the bit is republished (note:
                    // this skips the post-publish shutdown reclaim; acceptable for an
                    // unreachable path).
                    publish_bit(m);
                    return;  // (lock still held on false; guard releases it)
                }
                lk.lock();  // hand_off released the lock on success; continue serving
            }
        }

        /// FIFO direct handoff. Returns true (and releases the lock) iff a waiter
        /// consumed the node.
        [[nodiscard]] bool hand_off_locked(const std::size_t slot, std::unique_lock<std::mutex>& lk) noexcept {
            if constexpr (Mode == Wait::sync) {
                if (waiters_.empty()) {
                    return false;
                }
                SyncWaiter* w = waiters_.front();
                waiters_.pop_front();
                n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                w->assigned = slot;
                w->ready    = true;
                w->cv.notify_one();
                lk.unlock();
                return true;
            } else {
                return async_hand_off_locked(slot, lk);
            }
        }

        [[nodiscard]] bool async_hand_off_locked(const std::size_t slot, std::unique_lock<std::mutex>& lk) noexcept
            requires(Mode == Wait::async)
        {
            while (!waiters_.empty()) {
                std::shared_ptr<AsyncWaiter> w = std::move(waiters_.front());
                waiters_.pop_front();
                n_waiters_.fetch_sub(1, std::memory_order_seq_cst);
                if (bool expected = false;
                    !w->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    continue;  // timer/cancel claimed it first (its erase is pending/no-op)
                }
                w->timer.cancel();  // best effort; the claim already decides the race
                boost::asio::any_completion_handler<void(std::optional<Handle>)> handler = std::move(w->handler);
                const boost::asio::any_io_executor exec                                  = std::move(w->exec);
                lk.unlock();
                boost::asio::post(exec,
                                  [h  = std::move(handler),
                                   hd = std::optional<Handle>{
                                       Handle{this, slot_ptr(slot), slot}
                }]() mutable { std::move(h)(std::move(hd)); });
                return true;
            }
            return false;
        }

        /// Complete every parked waiter as shut down.
        void drain_waiters_locked(std::unique_lock<std::mutex>& lk) noexcept {
            if constexpr (Mode == Wait::sync) {
                for (SyncWaiter* w : waiters_) {
                    w->ready = true;  // assigned stays null -> caller sees shutdown
                    w->cv.notify_one();
                }
                waiters_.clear();
                n_waiters_.store(0, std::memory_order_seq_cst);
                (void)lk;
            } else {
                async_drain_locked(lk);
            }
        }

        void async_drain_locked(std::unique_lock<std::mutex>& lk) noexcept
            requires(Mode == Wait::async)
        {
            std::vector<std::pair<boost::asio::any_io_executor,
                                  boost::asio::any_completion_handler<void(std::optional<Handle>)>>>
                pending;
            for (const WaiterT& w : waiters_) {
                if (bool expected = false;
                    !w->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    continue;  // a timer already owns it and will complete it
                }
                w->timer.cancel();
                pending.emplace_back(std::move(w->exec), std::move(w->handler));
            }
            waiters_.clear();
            n_waiters_.store(0, std::memory_order_seq_cst);
            lk.unlock();
            for (auto& [exec, handler] : pending) {
                boost::asio::post(exec, [h = std::move(handler)]() mutable { std::move(h)(std::nullopt); });
            }
            lk.lock();  // shutdown() continues destroying free nodes under the lock
        }

        const std::size_t capacity_;
        Factory make_;

        mutable std::mutex mtx_;
        std::unique_ptr<Slot[]> storage_;  ///< capacity_ inline slots; allocated once, never moves.

        // ---- Idle tracking: lock-free bitset + waiter-count release gate ----
        //
        // Bit i SET  <=>  slot i is live and its `T` was constructed before the bit
        //                 was ever published — claimable by anyone via a lock-free CAS
        //                 (fast path).
        // Slots handed directly to a parked waiter never pass through the bitset.
        //
        // Release-side gate (the Go-sema / futex idiom): a releaser first checks
        // n_waiters_ (seq_cst) and hands the slot to the FIFO head under the mutex
        // if anyone is parked — the bit is never published, so nobody barges past
        // the queue. Otherwise it publishes the bit (fetch_or, seq_cst) and
        // re-checks n_waiters_ (seq_cst). A parker increments n_waiters_ (seq_cst)
        // and THEN re-scans the bitset with seq_cst loads before parking. In the
        // seq_cst total order the store-buffering outcome (parker misses the bit
        // AND releaser misses the count) is impossible, so no wakeup is lost.
        //
        // spare_slot_ids_ is written under mtx_ only; a claimer may dereference
        // slot_ptr(i) lock-free AFTER winning bit i (slot i is live and its `T` was
        // constructed before the bit was ever published). A slot with its bit set
        // is never destroyed: destruction requires first claiming the bit
        // (shutdown sweep, post-shutdown release reclaim) or holding the slot.
        struct alignas(std::hardware_destructive_interference_size) PaddedWord {
            std::atomic<std::uint64_t> bits{0};
        };
        static constexpr std::size_t bits_per_word = 64;

        std::vector<PaddedWord> words_;            ///< ceil(capacity/64) claimable-idle bits.
        std::vector<std::size_t> spare_slot_ids_;  ///< Unassigned slot indices; under mtx_.
        std::atomic<std::size_t> n_waiters_{0};    ///< Release-side gate; seq_cst everywhere.

        std::deque<WaiterT> waiters_;  ///< Parked acquirers, FIFO across the whole pool.
        std::size_t created_ = 0;      ///< Live slots + in-flight creations.
        std::atomic<bool> shutdown_{false};
    };

}  // namespace menagerie::starling
