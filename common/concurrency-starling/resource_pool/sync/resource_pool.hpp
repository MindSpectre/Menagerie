#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <menagerie/beaver>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <event_count.hpp>
#include <pause.hpp>

#include "detail/lease.hpp"

namespace menagerie::starling {

    /// A factory yields a @c T for a given slot index, or ignores the index.
    template <typename F, typename T>
    concept ResourceFactory =
        (std::invocable<F, std::size_t> && std::convertible_to<std::invoke_result_t<F, std::size_t>, T>) ||
        (std::invocable<F> && std::convertible_to<std::invoke_result_t<F>, T>);

    namespace detail {
        /// Per-thread start-word hint, seeded once, to spread bitset CAS contention.
        inline std::size_t resource_pool_word_hint() noexcept {
            const thread_local std::size_t hint = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return hint;
        }
    }  // namespace detail

    /**
     * @brief A bounded pool of up to @p MaxSize interchangeable @c T resources.
     *
     * Storage is entirely inline (no heap). Slots are acquired with a lock-free bitset
     * scan (@c try_acquire) or a bounded wait (@c acquire_for) and handed out as
     * move-only @c Lease<T> objects that release on destruction.
     *
     * ## Thread safety
     *
     * - @c try_acquire and @c acquire_for are safe to call from any number of threads
     *   concurrently.
     * - @c try_claim_free_for_repair and @c mark_healthy_free are safe to interleave with
     *   concurrent acquirers (typically called from a dedicated repair/janitor thread).
     * - Outstanding @c Lease<T> handles must not outlive the pool - destroy / release
     *   them before the @c ResourcePool destructor runs.
     * - Construction and destruction of the @c ResourcePool itself are NOT thread-safe.
     *
     * @tparam T        the pooled resource type (need not be default-constructible).
     * @tparam MaxSize  the inline capacity cap; @c n must not exceed it.
     */
    template <typename T, std::size_t MaxSize>
    class ResourcePool : beaver::Immutable {
        static_assert(MaxSize > 0, "ResourcePool MaxSize must be non-zero");
        static_assert(std::move_constructible<T> || std::copy_constructible<T>,
                      "ResourcePool<T>: T must be move- or copy-constructible "
                      "(the factory's return value is constructed into the slot)");

        static constexpr std::size_t bits_per_word = 64;

    public:
        using value_type = T;  ///< The pooled resource type.

        static constexpr std::size_t max_size   = MaxSize;  ///< Compile-time capacity cap.
        static constexpr std::size_t word_count = (MaxSize + bits_per_word - 1) / bits_per_word;  ///< Number of 64-bit free-bitset words.

        /**
         * @brief Construct a pool of @p n slots with an explicit spin budget: the
         *        canonical constructor every other overload funnels into.
         *
         * The factory is invoked once per slot ([0, n)); if it throws while building
         * slot k, the already-built [0, k) slots are destroyed before rethrowing,
         * leaving no partially-built pool and no leak.
         *
         * @throw std::invalid_argument if `n` exceeds `MaxSize`.
         * @throw whatever `make` throws, propagated after cleaning up any slots
         *        already constructed.
         */
        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr explicit ResourcePool(const std::size_t n, const std::chrono::nanoseconds spin_budget, Factory&& make)
            : n_{n},
              n_words_{(n + bits_per_word - 1) / bits_per_word},
              spin_budget_{spin_budget} {
            if (n > MaxSize) {
                throw std::invalid_argument{"ResourcePool: n exceeds MaxSize"};
            }

            std::size_t built = 0;
            try {
                for (; built < n_; ++built) {
                    if constexpr (std::invocable<Factory&, std::size_t>) {
                        std::construct_at(slot_ptr(built), std::invoke(make, built));
                    } else {
                        std::construct_at(slot_ptr(built), std::invoke(make));
                    }
                }
            } catch (...) {
                for (std::size_t i = 0; i < built; i++) {
                    std::destroy_at(slot_ptr(i));
                }
                throw;
            }

            // Bits [0, n_) start set: bit == 1 means "free AND healthy".
            for (std::size_t w = 0; w < n_words_; ++w) {
                const std::size_t base  = w * bits_per_word;
                const std::size_t count = (n_ - base < bits_per_word) ? n_ - base : bits_per_word;
                const std::uint64_t mask =
                    (count >= bits_per_word) ? ~std::uint64_t{0} : ((std::uint64_t{1} << count) - 1);
                free_words_[w].bits.store(mask, std::memory_order_relaxed);
            }
        }

        /// Construct a pool of @p n slots with the default spin budget.
        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr explicit ResourcePool(const std::size_t n, Factory&& make)
            : ResourcePool{n, std::chrono::nanoseconds{400}, std::forward<Factory>(make)} {
        }

        /// Construct a full pool (n == MaxSize) with the default spin budget.
        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr explicit ResourcePool(Factory&& make)
            : ResourcePool{MaxSize, std::chrono::nanoseconds{400}, std::forward<Factory>(make)} {
        }

        ~ResourcePool() noexcept {
            for (std::size_t i = 0; i < n_; ++i) {
                std::destroy_at(slot_ptr(i));
            }
        }

        /// Number of slots (`n` as passed to the constructor).
        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return n_;
        }

        /**
         * @brief Try to take a free slot without waiting.
         * @return a Lease on success, std::nullopt if no free slot is available.
         *
         * Lock-free: a thread-local start word spreads contention; each candidate bit is
         * claimed with a CAS that flips it 1 -> 0 (acquire on success, synchronizing with
         * the release in Lease's destructor / mark_healthy_free).
         */
        [[nodiscard]] std::optional<Lease<T>> try_acquire() noexcept {
            if (n_words_ == 0) {
                return std::nullopt;
            }
            const std::size_t start = detail::resource_pool_word_hint() % n_words_;
            for (std::size_t k = 0; k < n_words_; ++k) {
                const std::size_t w              = (start + k) % n_words_;
                std::atomic<std::uint64_t>& word = free_words_[w].bits;
                std::uint64_t cur                = word.load(std::memory_order_relaxed);
                while (cur != 0) {
                    if (const std::uint64_t bit = cur & (~cur + 1); word.compare_exchange_weak(
                            cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                        const std::size_t idx = w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bit));
                        return Lease<T>{slot_ptr(idx), &word, bit, &free_waiters_};
                    }
                    // compare_exchange_weak reloaded `cur` on failure.
                }
            }
            return std::nullopt;
        }

        /**
         * @brief Take a free slot, waiting up to @p timeout.
         * @return a Lease on success, std::nullopt if the timeout elapses first.
         *
         * Fast path (try_acquire), then a spin phase (pause_arc_agnostic) bounded by
         * min(spin_budget, deadline), then an EventCount park phase until the deadline.
         * `deadline` is derived once from `t0 + timeout` and never recomputed.
         */
        [[nodiscard]] std::optional<Lease<T>> acquire_for(const std::chrono::nanoseconds timeout) noexcept {
            if (std::optional<Lease<T>> lease = try_acquire()) {
                return lease;
            }

            const auto t0       = std::chrono::steady_clock::now();
            const auto deadline = t0 + timeout;
            const auto spin_end = t0 + spin_budget_;

            // Spin phase.
            while (std::chrono::steady_clock::now() < (spin_end < deadline ? spin_end : deadline)) {
                pause_arc_agnostic();
                if (std::optional<Lease<T>> lease = try_acquire()) {
                    return lease;
                }
            }

            // Park phase.
            for (;;) {
                const std::uint32_t key = free_waiters_.prepare_wait();
                if (std::optional<Lease<T>> lease = try_acquire()) {
                    free_waiters_.cancel_wait();
                    return lease;
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    free_waiters_.cancel_wait();
                    return std::nullopt;
                }
                free_waiters_.wait_until(key, deadline);  // consumes the registration
            }
        }

        /**
         * @brief Try to claim slot @p i for repair, racing acquirers.
         * @return true if the slot was free and is now claimed (its bit is 0, "down");
         *         false if a leaseholder currently has it (the caller should retry).
         *
         * Same CAS mechanism as try_acquire, aimed at one specific slot.
         */
        [[nodiscard]] bool try_claim_free_for_repair(const std::size_t i) noexcept {
            assert(i < n_ && "ResourcePool::try_claim_free_for_repair index out of range");
            std::atomic<std::uint64_t>& word = free_words_[i / bits_per_word].bits;
            const std::uint64_t bit          = std::uint64_t{1} << (i % bits_per_word);
            std::uint64_t cur                = word.load(std::memory_order_relaxed);
            while ((cur & bit) != 0) {
                if (word.compare_exchange_weak(cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Return slot @p i to circulation (sets its bit, notifies one waiter).
         *
         * Used by the system thread after reconstructing a repaired slot. Identical to
         * the release performed by Lease's destructor.
         */
        void mark_healthy_free(const std::size_t i) noexcept {
            assert(i < n_ && "ResourcePool::mark_healthy_free index out of range");
            const std::uint64_t bit = std::uint64_t{1} << (i % bits_per_word);
            free_words_[i / bits_per_word].bits.fetch_or(bit, std::memory_order_release);
            free_waiters_.notify_one();
        }

    private:
        /// Cache-line-isolated, write-hot bitset word.
        struct alignas(std::hardware_destructive_interference_size) PaddedWord {
            std::atomic<std::uint64_t> bits{0};
        };

        /// Inline uninitialized aligned storage for one T. The pool placement-constructs
        /// exactly the live slots in the ctor and destroys them in the dtor, so T need
        /// not be default-constructible.
        union Slot {
            Slot() noexcept {
            }
            ~Slot() noexcept {
            }
            T value;
        };

        [[nodiscard]] T* slot_ptr(const std::size_t i) noexcept {
            return &storage_[i].value;
        }

        const std::size_t n_;
        const std::size_t n_words_;

        std::array<Slot, MaxSize> storage_{};
        std::array<PaddedWord, word_count> free_words_{};

        /// Spin budget for acquire_for before parking on the EventCount.
        std::chrono::nanoseconds spin_budget_;
        EventCount free_waiters_{};
    };

}  // namespace menagerie::starling
