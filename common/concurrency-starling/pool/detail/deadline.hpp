#pragma once

#include <chrono>

namespace menagerie::starling::detail {

    [[nodiscard]] constexpr std::chrono::steady_clock::time_point
    pool_deadline_after(const std::chrono::steady_clock::time_point now,
                        const std::chrono::steady_clock::duration timeout) noexcept {
        // Each guarded subtraction is representable. Check before adding
        // so even duration::min()/max() cannot overflow the absolute clock.
        if (timeout > std::chrono::steady_clock::duration::zero() &&
            now.time_since_epoch() > std::chrono::steady_clock::duration::max() - timeout) [[unlikely]] {
            return std::chrono::steady_clock::time_point::max();
        }
        if (timeout < std::chrono::steady_clock::duration::zero() &&
            now.time_since_epoch() < std::chrono::steady_clock::duration::min() - timeout) [[unlikely]] {
            return std::chrono::steady_clock::time_point::min();
        }
        return now + timeout;
    }

}  // namespace menagerie::starling::detail
