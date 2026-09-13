#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <expected>
#include <mutex>
#include <optional>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/intrusive_ptr.hpp>

#include "borrowed.hpp"
#include "lifetime.hpp"
#include "waiter.hpp"

namespace menagerie::starling::detail {

    template <typename T>
    class PoolAsyncAcquire {
        template <typename Handler>
        struct AsyncWaiter final : pool_waiters::AsyncWaiterNode {
            AsyncWaiter(Pool<T>* owner,
                        const std::chrono::steady_clock::time_point absolute_deadline,
                        Handler completion)
                : AsyncWaiterNode{&post_completion},
                  pool{owner},
                  executor{boost::asio::get_associated_executor(completion)},
                  timer{std::in_place, executor},
                  context_waiters{
                      boost::asio::use_service<pool_waiters::AsyncWaiterService>(executor.context()).waiters},
                  cancellation{boost::asio::get_associated_cancellation_slot(completion)},
                  handler{std::move(completion)} {
                this->deadline = absolute_deadline;
                if (this->bounded()) {
                    timer->expires_at(absolute_deadline);
                }
            }

            void register_cancellation() {
                if (cancellation.is_connected()) {
                    struct Registration {
                        boost::intrusive_ptr<AsyncWaiter> self;

                        explicit Registration(AsyncWaiter* waiter) noexcept
                            : self{waiter} {
                        }
                        Registration(Registration&&) noexcept = default;
                        ~Registration() {
                            // A signal may be destroyed before the context.
                            // Its child slot then ceases to exist as well.
                            if (self) {
                                self->cancellation = {};
                            }
                        }

                        void operator()(const boost::asio::cancellation_type_t type) const {
                            if ((type & boost::asio::cancellation_type::terminal) !=
                                boost::asio::cancellation_type::none) {
                                self->select(pool_waiters::WaiterState::cancelled);
                            }
                        }
                    };
                    cancellation.emplace<Registration>(this);
                }
            }

            // Initiation holds pool->mutex_ and links the node before arming.
            // Thus terminal selection cannot race a later timer registration.
            void start_timer_locked() {
                if (this->bounded()) {
                    timer->async_wait(
                        [self = boost::intrusive_ptr<AsyncWaiter>{this}](const boost::system::error_code& error) {
                            if (!error) {
                                self->select(pool_waiters::WaiterState::timeout);
                            }
                        });
                }
            }

            void prepare_abandon() noexcept override {
                boost::intrusive_ptr<AsyncWaiter> membership;
                {
                    const std::lock_guard lock{pool->mutex_};
                    if (this->hook.is_linked()) {
                        pool->unlink_waiter(*this, pool_waiters::WaiterState::cancelled);
                        membership = boost::intrusive_ptr<AsyncWaiter>{this, false};
                    }
                }
                // The service still owns this node. Release the queue reference
                // after unlocking; never destroy the payload under the pool mutex.
            }

            void cleanup() noexcept {
                // Normal delivery is on the awaiting executor; context shutdown
                // requires that its I/O threads have stopped (Asio's contract).
                // Clear the slot while the handler still owns its coroutine.
                // Clearing destroys Registration, which disarms our saved slot.
                // Use a local slot so this cannot mutate clear()'s own object.
                auto slot = std::exchange(cancellation, {});
                slot.clear();
                timer.reset();
            }

            void abandon() noexcept override {
                cleanup();
                // A handoff may already have selected a slot before the context
                // stopped. Its borrow must be returned even without delivery.
                auto outcome = result();
                handler.reset();
                // A selecting borrower can still hold this node after context
                // destruction. Drop executor work ownership while it is alive.
                executor = {};
            }

            void select(const pool_waiters::WaiterState terminal) noexcept {
                std::unique_lock lock{pool->mutex_};
                if (this->state != pool_waiters::WaiterState::queued) {
                    return;
                }
                if (!this->hook.is_linked()) {
                    // Cancellation may arrive between slot registration and
                    // parking. The initiator observes this rejection under the
                    // same mutex and remains responsible for its completion.
                    assert(terminal == pool_waiters::WaiterState::cancelled);
                    this->state = terminal;
                    return;
                }
                pool->complete_waiter_locked(*this, terminal, lock);
            }

            [[nodiscard]] std::expected<PoolBorrowed<T>, AcquireError> result() const noexcept {
                switch (this->state) {
                    case pool_waiters::WaiterState::assigned:
                        return PoolBorrowed<T>{pool, this->assigned_slot};
                    case pool_waiters::WaiterState::timeout:
                        return std::unexpected{AcquireError::timeout};
                    case pool_waiters::WaiterState::cancelled:
                        return std::unexpected{AcquireError::cancelled};
                    case pool_waiters::WaiterState::shutdown:
                        return std::unexpected{AcquireError::shutdown};
                    case pool_waiters::WaiterState::queued:
                        std::unreachable();
                }
                std::unreachable();
            }

            static void post_completion(boost::intrusive_ptr<pool_waiters::AsyncWaiterNode> selected) noexcept {
                // The selected queue reference becomes the posted operation's
                // reference without any intervening release or new allocation.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                boost::intrusive_ptr<AsyncWaiter> self{static_cast<AsyncWaiter*>(selected.detach()), false};
                post_completion_impl<false>(std::move(self));
            }

            template <bool Initiating>
            static void post_completion_impl(boost::intrusive_ptr<AsyncWaiter> self) noexcept(!Initiating) {
                // The posted callback may release the waiter before submit()
                // returns, so retain the gate independently for this call.
                const auto context_waiters = self->context_waiters;
                auto schedule              = [&] {
                    const auto awaiting_executor = self->executor;
                    auto deliver                 = [self = std::move(self)]() mutable {
                        self->context_waiters->remove(*self);
                        self->cleanup();
                        auto outcome    = self->result();
                        auto completion = std::move(*self->handler);
                        self->handler.reset();
                        completion(std::move(outcome));
                    };
                    boost::asio::post(awaiting_executor, std::move(deliver));
                };
                if constexpr (Initiating) {
                    context_waiters->submit_initiation(schedule);
                } else {
                    context_waiters->submit(schedule);
                }
            }

            Pool<T>* pool;
            boost::asio::any_io_executor executor;
            // Construct the timer service BEFORE the lifecycle service: Asio
            // shuts services down in reverse registration order. Our shutdown
            // destroys timers/frames while their services are still alive.
            std::optional<boost::asio::steady_timer> timer;
            boost::intrusive_ptr<pool_waiters::AsyncWaiterContext> context_waiters;
            boost::asio::cancellation_slot cancellation;
            std::optional<Handler> handler;
        };

        template <typename Handler>
        static void post_immediate(Handler handler, std::expected<PoolBorrowed<T>, AcquireError> result) {
            const auto executor = boost::asio::get_associated_executor(handler);
            boost::asio::post(executor, [handler = std::move(handler), result = std::move(result)]() mutable {
                handler(std::move(result));
            });
        }

        template <typename Handler>
        static void initiate_async_acquire(Pool<T>* pool,
                                           const std::chrono::steady_clock::time_point deadline,
                                           const bool cancelled,
                                           Handler handler) {
            const auto expired = [&] {
                return deadline != std::chrono::steady_clock::time_point::max() &&
                       std::chrono::steady_clock::now() >= deadline;
            };
            if (cancelled) {
                post_immediate(std::move(handler), std::unexpected{AcquireError::cancelled});
                return;
            }
            if (pool->closed_.load(std::memory_order_acquire)) {
                post_immediate(std::move(handler), std::unexpected{AcquireError::shutdown});
                return;
            }
            if (expired()) {
                post_immediate(std::move(handler), std::unexpected{AcquireError::timeout});
                return;
            }
            if (const auto index = pool->claim_idle()) {
                if (pool->closed_.load(std::memory_order_acquire)) {
                    {
                        const std::lock_guard lock{pool->mutex_};
                        pool->quarantine_borrowed(*index);
                    }
                    post_immediate(std::move(handler), std::unexpected{AcquireError::shutdown});
                } else if (expired()) {
                    pool->handoff_or_publish(*index);
                    post_immediate(std::move(handler), std::unexpected{AcquireError::timeout});
                } else {
                    post_immediate(std::move(handler), PoolBorrowed<T>{pool, *index});
                }
                return;
            }

            boost::intrusive_ptr<AsyncWaiter<Handler>> waiter{
                new AsyncWaiter<Handler>{pool, deadline, std::move(handler)}
            };
            waiter->register_cancellation();
            waiter->context_waiters->add(*waiter);
            std::unique_lock lock{pool->mutex_};
            if (waiter->state == pool_waiters::WaiterState::cancelled) {
                // The cancellation callback already selected this rejection.
            } else if (pool->closed_.load(std::memory_order_relaxed)) {
                waiter->state = pool_waiters::WaiterState::shutdown;
            } else if (expired()) {
                waiter->state = pool_waiters::WaiterState::timeout;
            } else {
                pool->waiter_gate_.fetch_add(1, std::memory_order_seq_cst);
                if (const auto index = pool->claim_idle(true)) {
                    pool->waiter_gate_.fetch_sub(1, std::memory_order_seq_cst);
                    if (expired()) {
                        waiter->state = pool_waiters::WaiterState::timeout;
                        pool->handoff_or_publish_locked(*index, lock);
                    } else {
                        waiter->assigned_slot = *index;
                        waiter->state         = pool_waiters::WaiterState::assigned;
                    }
                } else {
                    pool->waiters_.push_back(*waiter);
                    pool->assert_invariants_locked();
                    // One reference belongs to FIFO membership until exact-node
                    // unlink transfers it to complete_waiter_locked's local.
                    intrusive_ptr_add_ref(waiter.get());
                    try {
                        waiter->start_timer_locked();
                    } catch (...) {
                        // An Asio initiation allocation failure must not leave
                        // a linked node, waiter gate, or cancellation owner.
                        pool->unlink_waiter(*waiter, pool_waiters::WaiterState::cancelled);
                        const boost::intrusive_ptr<AsyncWaiter<Handler>> membership{waiter.get(), false};
                        lock.unlock();
                        waiter->cleanup();
                        waiter->context_waiters->remove(*waiter);
                        throw;
                    }
                    return;
                }
            }
            if (lock.owns_lock()) {
                lock.unlock();
            }
            try {
                // Never linked: the initiating caller can still recover from
                // a scheduling failure. Retain its own reference across post,
                // whose failed operation may already have released its copy.
                AsyncWaiter<Handler>::template post_completion_impl<true>(waiter);
            } catch (...) {
                // submit_initiation has released the registry mutex. Disarm
                // cancellation/timer ownership and remove lifecycle membership
                // before releasing the handler or rolling back an assigned slot.
                waiter->cleanup();
                waiter->context_waiters->remove(*waiter);
                auto outcome = waiter->result();
                waiter->handler.reset();
                throw;
            }
        }

    public:
        static boost::asio::awaitable<std::expected<PoolBorrowed<T>, AcquireError>>
        acquire_until(AsyncPoolReference<Pool<T>> async_pool, const std::chrono::steady_clock::time_point deadline) {
            auto pool               = async_pool.pool;
            const auto cancellation = co_await boost::asio::this_coro::cancellation_state;
            const bool cancelled    = (cancellation.cancelled() & boost::asio::cancellation_type::terminal) !=
                                   boost::asio::cancellation_type::none;
            boost::asio::use_awaitable_t<> token;
            co_return co_await boost::asio::async_initiate<boost::asio::use_awaitable_t<>,
                                                           void(std::expected<PoolBorrowed<T>, AcquireError>)>(
                [pool = pool, deadline, cancelled](auto handler) mutable {
                    initiate_async_acquire(pool, deadline, cancelled, std::move(handler));
                },
                token);
        }
    };

}  // namespace menagerie::starling::detail
