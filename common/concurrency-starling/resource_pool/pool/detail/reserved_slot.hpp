#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <menagerie/beaver>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace menagerie::starling::detail {

    /// Exclusive arena slot. Destruction rolls back; only publish transfers it.
    template <typename T>
    class PoolReservedSlot : beaver::NonCopyable {
    public:
        PoolReservedSlot(PoolReservedSlot&& other) noexcept
            : pool_{std::exchange(other.pool_, {})},
              index_{other.index_},
              constructed_{std::exchange(other.constructed_, false)} {
        }

        PoolReservedSlot& operator=(PoolReservedSlot&& other) noexcept {
            if (this != &other) {
                rollback();
                pool_        = std::exchange(other.pool_, {});
                index_       = other.index_;
                constructed_ = std::exchange(other.constructed_, false);
            }
            return *this;
        }

        ~PoolReservedSlot() noexcept {
            rollback();
        }

        [[nodiscard]] T* get() noexcept {
            return constructed_ ? address() : nullptr;
        }
        [[nodiscard]] const T* get() const noexcept {
            return constructed_ ? address() : nullptr;
        }

        template <typename... Args>
        T& emplace(Args&&... args) {
            assert(has_pool());
            assert(!constructed_);
            T* result    = std::construct_at(address(), std::forward<Args>(args)...);
            constructed_ = true;
            return *result;
        }

        /// Factory must throw before construction or immediately return the
        /// constructed final address. Constructing and then throwing violates
        /// the transaction contract; the pool cannot infer that lifetime.
        template <typename FactoryT>
        T& construct(FactoryT&& factory) {
            assert(has_pool());
            assert(!constructed_);
            T* const expected = address();
            T* const result   = std::invoke(std::forward<FactoryT>(factory), expected);
            if (result != expected) {
                throw std::invalid_argument{"Pool::ReservedSlot factory returned a different address"};
            }
            constructed_ = true;
            return *result;
        }

        void destroy() noexcept {
            if (constructed_) {
                std::destroy_at(address());
                constructed_ = false;
            }
        }

        [[nodiscard]] bool publish() noexcept {
            assert(has_pool());
            {
                std::unique_lock lock{pool_->mutex_};
                if (pool_->closed_.load(std::memory_order_relaxed)) {
                    return false;
                }
                assert(constructed_);
                assert(pool_->states_[index_] == Pool<T>::SlotState::reserved);
                pool_->states_[index_] = Pool<T>::SlotState::borrowed;
                --pool_->reserved_count_;
                ++pool_->usable_size_;
                pool_->assert_invariants_locked();
                pool_->handoff_or_publish_locked(index_, lock);
            }
            constructed_ = false;
            pool_        = {};
            return true;
        }

    private:
        friend class Pool<T>;
        PoolReservedSlot(Pool<T>* pool, const std::size_t index, const bool constructed = false) noexcept
            : pool_{pool},
              index_{index},
              constructed_{constructed} {
        }

        [[nodiscard]] bool has_pool() const noexcept {
            return pool_ != nullptr;
        }

        [[nodiscard]] T* address() const noexcept {
            return pool_->slot_ptr(index_);
        }

        void rollback() noexcept {
            if (!has_pool()) {
                return;
            }
            destroy();
            {
                std::lock_guard lock{pool_->mutex_};
                assert(pool_->states_[index_] == Pool<T>::SlotState::reserved);
                pool_->states_[index_] = Pool<T>::SlotState::vacant;
                pool_->vacant_.push_back(index_);
                --pool_->reserved_count_;
                pool_->assert_invariants_locked();
            }
            pool_ = {};
        }

        Pool<T>* pool_;
        std::size_t index_;
        bool constructed_ = false;
    };

}  // namespace menagerie::starling::detail
