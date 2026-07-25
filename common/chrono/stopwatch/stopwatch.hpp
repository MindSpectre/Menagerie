#pragma once

#include <chrono>
#include <utility>
#include <vector>

namespace menagerie::chrono {

    /**
     * @brief Flag-collecting stopwatch: start() opens a run, add_flag() records a
     *        timestamp, delta_t(i) returns (since_prev, since_start) for flag i.
     *
     * duration picks the reporting unit, clock the time source. Not thread-safe;
     * use one instance per measuring thread.
     */
    template <typename duration = std::chrono::milliseconds, typename clock = std::chrono::high_resolution_clock>
    class Stopwatch {
    public:
        using time_point = clock::time_point;  ///< This stopwatch's time point type.

        /// reserve_flags pre-sizes the flag vector to avoid reallocation while timing.
        explicit Stopwatch(std::size_t reserve_flags = 20) {
            flags_.reserve(reserve_flags);
        }

        /// Clears previous flags and records the starting timestamp.
        void start() noexcept {
            flags_.clear();
            add_flag();
        }

        /// Records a final flag and returns every recorded flag, clearing this stopwatch.
        std::vector<time_point> stop() noexcept {
            flags_.emplace_back(clock::now());
            std::vector<time_point> flags = std::move(flags_);
            flags_.clear();
            return flags;
        }

        /// Records a timestamp flag.
        void add_flag() {
            auto now_time = clock::now();
            flags_.emplace_back(now_time);
        }

        /// The raw time point recorded for flag i.
        time_point operator[](const std::size_t i) const {
            return flags_[i];
        }

        /// Returns {since_prev, since_start} for flag i; {zero, zero} if i is out of
        /// range or 0 (the start flag has no predecessor).
        [[nodiscard]] std::pair<duration, duration> delta_t(const std::size_t i) const {
            if (i >= flags_.size() || i == 0) {
                return {duration::zero(), duration::zero()};
            }
            const auto since_start = std::chrono::duration_cast<duration>(flags_[i] - flags_[0]);
            duration since_prev{duration::zero()};
            if (i >= 1) {
                since_prev = std::chrono::duration_cast<duration>(flags_[i] - flags_[i - 1]);
            }

            return {since_prev, since_start};
        }

        /// Time elapsed between flag 0 (start) and flag i.
        [[nodiscard]] duration from_start(const std::size_t i) const {
            return std::chrono::duration_cast<duration>(flags_[i] - flags_[0]);
        }
        /// Time elapsed between flag i-1 and flag i.
        [[nodiscard]] duration from_prev(const std::size_t i) const {
            return std::chrono::duration_cast<duration>(flags_[i] - flags_[i - 1]);
        }

        /// Time elapsed between flag 0 and the last recorded flag.
        [[nodiscard]] duration total_time() const {
            return std::chrono::duration_cast<duration>(flags_.back() - flags_[0]);
        }

        /// Every recorded flag, in recording order.
        [[nodiscard]] const std::vector<time_point>& get_flags() const {
            return flags_;
        }

        /// Mean of the per-flag deltas (total_time() divided by flags recorded minus one).
        [[nodiscard]] duration average_delta() const {
            duration total = duration::zero();
            for (std::size_t i = 1; i < flags_.size(); ++i) {
                auto d  = flags_[i] - flags_[i - 1];
                total  += std::chrono::duration_cast<duration>(d);
            }
            return total / (flags_.size() - 1);
        }

        /// Times a single invocation of func.
        template <typename Func>
        static duration measure(Func&& func) {
            auto start_time = clock::now();
            std::forward<Func>(func)();  // Execute the lambda function
            auto end_time = clock::now();
            return std::chrono::duration_cast<duration>(end_time - start_time);
        }

    private:
        std::vector<time_point> flags_;
    };

}  // namespace menagerie::chrono
