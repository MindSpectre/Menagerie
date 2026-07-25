#pragma once
#include "base_random_generator.hpp"
namespace menagerie::math::random {
    /// @brief Generates random integers using the shared std::mt19937 state.
    class NumberGenerator final : BaseRandomGenerator {
    public:
        NumberGenerator() = default;
        /// Seeds the generator from a caller-supplied std::mt19937.
        explicit NumberGenerator(const std::mt19937& r_generator)
            : BaseRandomGenerator(r_generator) {
        }
        /// Returns a value uniformly distributed in [min, max].
        [[nodiscard]] uint32_t generate_random_uint32(uint32_t min, uint32_t max) const;

        /// Returns an integral T uniformly distributed over T's full range.
        template <typename T>
        T generate_random_t() const
            requires std::is_integral_v<T>
        {
            std::uniform_int_distribution<T> distrib(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
            return distrib(generator_);
        }
    };

}  // namespace menagerie::math::random
