#pragma once
#include <chrono>
#include <concepts>
#include <future>
#include <menagerie/multithread>
#include <thread>
#include <utility>

#include "detail/cancellation_token.hpp"

namespace menagerie::chrono {
    /**
     * @brief Runs a callable with a deadline on an owned ThreadPool, in one of two
     *        enforcement modes.
     *
     * execute_polite_vanish() only ever asks the worker to stop -- a watchdog flips a
     * CancellationToken and returns, leaving the worker responsible for noticing and
     * unwinding; safe with locks held, but does nothing if the callable never checks
     * the token. execute_violent_kill() enforces the deadline from the outside
     * (TerminateThread/pthread_cancel) but is undefined behavior if the target thread
     * holds a lock at cancellation time. Prefer execute_polite_vanish() unless the
     * callable is known-uncooperative.
     */
    class Timer : beavers::NonCopyable {
    public:
        /// Builds an owned ThreadPool from config.
        explicit Timer(const multithread::ThreadPoolConfig& config) {
            pool_ = std::make_shared<multithread::ThreadPool>(config);
        }

        /// Runs tasks on an existing, possibly shared, pool.
        explicit Timer(std::shared_ptr<multithread::ThreadPool> pool)
            : pool_(std::move(pool)) {
        }

        using clock = std::chrono::steady_clock;  ///< Clock used to measure the deadline.

        /**
         * @brief Runs fn(args...) on the pool; if it has not finished by timeout, asks
         *        it to stop cooperatively by cancelling a CancellationToken.
         *
         * args... must include a std::shared_ptr<CancellationToken> (checked with a
         * compile-time static_assert) that fn is expected to poll via
         * CancellationToken::stop_requested() and honor. Cancelling the token does not
         * forcibly stop fn -- see execute_violent_kill() for that.
         * @return A std::future for fn's result.
         */
        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute_polite_vanish(std::chrono::milliseconds timeout, Callable&& fn, Args&&... args);

        /**
         * @brief Runs fn(args...) on a raw std::thread; if it has not finished by
         *        timeout, force-kills that thread (TerminateThread on Windows,
         *        pthread_cancel on POSIX) regardless of what fn is doing.
         *
         * Undefined behavior if the target thread holds a lock (or owns any other
         * process-wide resource) at the moment it is killed -- a killed thread never
         * runs destructors or releases what it holds. Prefer execute_polite_vanish()
         * unless fn is known not to cooperate with cancellation.
         * @return A std::future for fn's result.
         */
        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute_violent_kill(std::chrono::milliseconds timeout,
                                  const std::shared_ptr<CancellationToken>& token,
                                  Callable&& fn,
                                  Args&&... args);

    private:
        std::shared_ptr<multithread::ThreadPool> pool_;

        // helper - default spawns a jthread; replace with thread-pool later
        template <typename F>
        auto spawn(F&& f) {
            return std::jthread{std::forward<F>(f)};
        }

        // type-trait: does first arg look like a cancellation token?
    };
}  // namespace menagerie::chrono

#include "detail/timer.inl"
