#pragma once

#include <cstdint>

namespace menagerie::multithread {
    /// Spin iterations a wait strategy attempts before falling back to a
    /// cheaper-but-slower wait (e.g. yield or block).
    static constexpr std::uint16_t SPIN_BEFORE_YIELD = 100;
}
