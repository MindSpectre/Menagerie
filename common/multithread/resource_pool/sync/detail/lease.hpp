#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <menagerie/beavers>

#include <event_count.hpp>

namespace menagerie::multithread {

    /**
     * @brief Move-only RAII handle for a free-pool slot.
     *
     * Holds only what release touches, so the type is genuinely @c Lease<T> - it does
     * not carry the pool's @c MaxSize. The destructor (and move-assignment of an
     * occupied lease) sets the slot's bit with release ordering and notifies one
     * waiter. A moved-from lease has a null @c word_ and releases nothing.
     *
     * @warning A @c Lease<T> holds interior pointers into the @c ResourcePool's storage
     *          and into its @c EventCount waiter. It must NOT outlive the pool it was
     *          acquired from - destroy / release every outstanding lease before the
     *          pool's destructor runs. (Holding leases in containers that outlive the
     *          pool is a use-after-free.)
     *
     * @tparam T the pooled resource type.
     */
    template <typename T>
    class Lease : beavers::NonCopyable {
    public:
        using value_type = T;  ///< The leased resource type.

        Lease() noexcept = default;

        /// Wraps the resource pointer and the free-bitset bit/word the destructor
        /// will release into (used by ResourcePool internally).
        Lease(T* resource, std::atomic<std::uint64_t>* word, const std::uint64_t bit, EventCount* waiters) noexcept
            : resource_{resource},
              word_{word},
              bit_{bit},
              waiters_{waiters} {
        }

        /// Transfers ownership from `other`, leaving it moved-from (releases nothing).
        Lease(Lease&& other) noexcept
            : resource_{other.resource_},
              word_{other.word_},
              bit_{other.bit_},
              waiters_{other.waiters_} {
            other.word_ = nullptr;
        }

        /// Releases any currently-held slot, then transfers ownership from `other`.
        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                release();
                resource_   = other.resource_;
                word_       = other.word_;
                bit_        = other.bit_;
                waiters_    = other.waiters_;
                other.word_ = nullptr;
            }
            return *this;
        }

        ~Lease() {
            release();
        }

        /// Dereferences the leased resource.
        [[nodiscard]] T& operator*() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from Lease");
            return *resource_;
        }

        /// Accesses a member of the leased resource.
        [[nodiscard]] T* operator->() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from Lease");
            return resource_;
        }

        /// Returns the raw pointer to the leased resource.
        [[nodiscard]] T* get() const noexcept {
            return resource_;
        }

    private:
        void release() noexcept {
            if (word_ != nullptr) {
                word_->fetch_or(bit_, std::memory_order_release);
                waiters_->notify_one();
                word_ = nullptr;
            }
        }

        T* resource_                      = nullptr;
        std::atomic<std::uint64_t>* word_ = nullptr;
        std::uint64_t bit_                = 0;
        EventCount* waiters_              = nullptr;
    };

}  // namespace menagerie::multithread
