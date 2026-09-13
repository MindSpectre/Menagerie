#pragma once

#include <cstddef>
#include <cstdint>

namespace menagerie::starling {

    template <typename T>
    class Pool;

    enum class AcquireError : std::uint8_t { exhausted, timeout, cancelled, shutdown };

    struct PoolStats {
        std::size_t capacity    = 0;
        std::size_t vacant      = 0;
        std::size_t reserved    = 0;
        std::size_t idle        = 0;
        std::size_t borrowed    = 0;
        std::size_t quarantined = 0;
        std::size_t waiters     = 0;
    };

}  // namespace menagerie::starling
