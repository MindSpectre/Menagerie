#pragma once

#include <algorithm>
#include <chrono>
#include <menagerie/beavers>
#include <thread>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace menagerie::chrono {
    /// Exponential backoff: base * 2^attempt, clamped to cap. attempt is 0-based
    /// (attempt 0 -> base). The shift is clamped before the multiply so `1u << shift`
    /// can never overflow, regardless of how large a cap the caller passes.
    [[nodiscard]] constexpr std::chrono::milliseconds
    exponential_backoff(const std::uint32_t attempt,
                        const std::chrono::milliseconds base,
                        const std::chrono::milliseconds cap) noexcept {
        const std::uint32_t shift = std::min(attempt, 20u);
        return std::min(base * (1u << shift), cap);
    }

    /// Blocks the calling thread for duration (std::this_thread::sleep_for).
    template <beavers::IsDuration DurationClass>
    void sleep_for(const DurationClass duration) {
        std::this_thread::sleep_for(duration);
    }

    /// Suspends the calling coroutine for duration, using the awaiting coroutine's
    /// executor (boost::asio::steady_timer under the hood).
    template <beavers::IsDuration DurationClass>
    boost::asio::awaitable<void> async_sleep_for(const DurationClass duration) {
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(executor);
        timer.expires_after(duration);
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

    /// @overload
    /// Runs the timer on executor instead of the awaiting coroutine's own executor.
    template <beavers::IsDuration DurationClass>
    boost::asio::awaitable<void> async_sleep_for(const boost::asio::any_io_executor& executor,
                                                 const DurationClass duration) {
        boost::asio::steady_timer timer(executor);
        timer.expires_after(duration);
        co_await timer.async_wait(boost::asio::use_awaitable);
    }
}  // namespace menagerie::chrono
