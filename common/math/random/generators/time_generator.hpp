#pragma once
#include <chrono>
#include <utility>

#include "base_random_generator.hpp"

namespace menagerie::math::random {

    /// @brief Generates random durations and dates using the shared
    /// std::mt19937 state.
    class RandomTimeGenerator final : public BaseRandomGenerator {
    public:
        RandomTimeGenerator() = default;
        /// Seeds the generator from a caller-supplied std::mt19937.
        explicit RandomTimeGenerator(const std::mt19937& r_generator)
            : BaseRandomGenerator(r_generator) {
        }
        /// @brief Generate a random time in range [ms * (100 - pc) / 100, ms * (100 + pc) / 100]. It means generating
        /// around specific time eg (100ms) and add random for this.
        /// @param target_ms The base time in milliseconds.
        /// @param deviation The percentage variation allowed. Default value is 15%
        /// @return A random time in milliseconds.
        /// @throws std::invalid_argument if deviation < 0 or deviation > 100 or target_ms < 0.
        [[nodiscard]] std::chrono::milliseconds generate_milliseconds(uint32_t target_ms, int8_t deviation = 15) const;

        /// Returns a random date uniformly distributed between from and to (inclusive).
        [[nodiscard]] std::chrono::year_month_day generate_date(const std::chrono::year_month_day& from,
                                                                const std::chrono::year_month_day& to) const;

        /// Returns a random date between from and the current date.
        [[nodiscard]] std::chrono::year_month_day
        generate_date_from_to_now(const std::chrono::year_month_day& from) const;

        /// Returns a random date between the start of the last century and to.
        [[nodiscard]] std::chrono::year_month_day generate_date_to(const std::chrono::year_month_day& to) const;

        /// Returns a random date between the start of the last century and now.
        [[nodiscard]] std::chrono::year_month_day generate_date_last_century() const;

        /// Returns a random (start, end) pair with start and end both in [from, to] and start <= end.
        [[nodiscard]] std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
        generate_time_period(const std::chrono::year_month_day& from, const std::chrono::year_month_day& to) const;

        /// Returns a random (start, end) pair anchored between the last century and now.
        [[nodiscard]] std::pair<std::chrono::year_month_day, std::chrono::year_month_day> generate_time_period() const;

        /// Returns a random (start, end) pair with both dates between from and now.
        [[nodiscard]] std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
        generate_time_period_from(const std::chrono::year_month_day& from) const;

        /// Returns a random (start, end) pair with both dates between the last century and to.
        [[nodiscard]] std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
        generate_time_period_to(const std::chrono::year_month_day& to) const;

    private:
        static constexpr std::chrono::year_month_day start_of_last_century_{
            std::chrono::year{1925}, std::chrono::month{7}, std::chrono::day{25}};
    };

}  // namespace menagerie::math::random
