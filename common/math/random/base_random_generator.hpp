#pragma once

#include <random>

/// Random-value toolkit built on std::mt19937.
namespace menagerie::math::random {
    /// @brief Owns and seeds the shared std::mt19937 state used by
    /// NumberGenerator and RandomTimeGenerator.
    ///
    /// generator_ is mutable so a generator can be held by const& or stored in
    /// a const object while each generate_* call still advances the state.
    class BaseRandomGenerator {
    public:
        /// Seeds the generator from std::random_device.
        BaseRandomGenerator()
            : generator_{std::random_device{}()} {
        }

        /// Seeds the generator from a caller-supplied std::mt19937.
        template <typename GeneratorTp>
            requires std::is_same_v<std::remove_cvref_t<GeneratorTp>, std::mt19937>
        explicit BaseRandomGenerator(GeneratorTp&& r_generator)
            : generator_{std::forward<GeneratorTp>(r_generator)} {
        }
        virtual ~BaseRandomGenerator() = default;

    protected:
        mutable std::mt19937 generator_;  ///< Shared Mersenne Twister state, mutable so const generate_* calls can still advance it.
    };
}  // namespace menagerie::math::random
