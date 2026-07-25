#pragma once

#include <atomic>
#include <menagerie/beavers>

namespace menagerie::chrono {
    /// Cooperative-stop flag: Timer::execute_polite_vanish() flips it via cancel() on
    /// timeout, and the running callable is expected to poll stop_requested() and
    /// unwind. Cancelling does not forcibly stop anything by itself.
    class CancellationToken : beavers::NonCopyable {
    public:
        /// Requests that the running callable stop.
        void cancel() noexcept {
            flag_.store(true, std::memory_order_release);
        }

        /// Clears a previous cancel(), for reusing the token across runs.
        void renew() noexcept {
            flag_.store(false, std::memory_order_release);
        }

        /// True if cancel() has been called since construction or the last renew().
        [[nodiscard]] bool stop_requested() const noexcept {
            return flag_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic_bool flag_{false};
    };

    /// Builds a fresh, non-cancelled CancellationToken.
    inline CancellationToken create_cancellation_token() {
        return CancellationToken{};
    }
}  // namespace menagerie::chrono
