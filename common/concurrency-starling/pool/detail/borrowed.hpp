#pragma once

#include <cassert>
#include <cstddef>
#include <menagerie/beaver>
#include <mutex>
#include <utility>

#include "../types.hpp"

namespace menagerie::starling::detail {

    template <typename T>
    class PoolSyncAcquire;

    template <typename T>
    class PoolAsyncAcquire;

    /// Exclusive borrow. Pointer access performs no ownership or status work.
    template <typename T>
    class PoolBorrowed : beaver::NonCopyable {
    public:
        PoolBorrowed(PoolBorrowed&& other) noexcept
            : pool_{std::exchange(other.pool_, {})},
              index_{other.index_},
              ptr_{std::exchange(other.ptr_, nullptr)} {
        }

        PoolBorrowed& operator=(PoolBorrowed&& other) noexcept {
            if (this != &other) {
                release_now();
                pool_  = std::exchange(other.pool_, {});
                index_ = other.index_;
                ptr_   = std::exchange(other.ptr_, nullptr);
            }
            return *this;
        }

        ~PoolBorrowed() noexcept {
            release_now();
        }

        [[nodiscard]] T* get() const noexcept {
            return ptr_;
        }

        [[nodiscard]] T* operator->() const noexcept {
            assert(ptr_ != nullptr);
            return ptr_;
        }

        [[nodiscard]] T& operator*() const noexcept {
            assert(ptr_ != nullptr);
            return *ptr_;
        }

        /// Consume this borrow without offering its constructed object to waiters.
        void quarantine() noexcept {
            if (ptr_ == nullptr)
                return;
            {
                std::lock_guard lock{pool_->mutex_};
                pool_->quarantine_borrowed(index_);
            }
            ptr_  = nullptr;
            pool_ = nullptr;
        }

    private:
        friend class Pool<T>;
        friend class PoolSyncAcquire<T>;
        friend class PoolAsyncAcquire<T>;

        PoolBorrowed(Pool<T>* pool, const std::size_t index) noexcept
            : pool_{pool},
              index_{index},
              ptr_{pool_->slot_ptr(index)} {
        }

        void release_now() noexcept {
            if (ptr_ == nullptr) {
                return;
            }
            pool_->handoff_or_publish(index_);
            ptr_  = nullptr;
            pool_ = {};
        }

        Pool<T>* pool_;
        std::size_t index_;
        T* ptr_;
    };

}  // namespace menagerie::starling::detail
