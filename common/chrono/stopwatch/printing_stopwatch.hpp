#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace menagerie::chrono {
    /**
     * @brief Named-flag stopwatch that prints its report to std::cout on print()/finish()
     *        and automatically in its destructor.
     *
     * T is the reporting time unit (milliseconds by default); flag(name) (or
     * operator++/operator-- for unnamed add/remove) records a timestamp,
     * set_countdown_from_prev()/set_countdown_from_start() control which deltas the
     * report includes.
     */
    template <typename T = std::chrono::milliseconds>
    class PrintingStopwatch {
    public:
        /// True if the report includes the delta since the previous flag.
        [[nodiscard]] bool is_count_from_prev() const {
            return countdown_from_prev_;
        }

        /// Sets whether the report includes the delta since the previous flag.
        void set_countdown_from_prev(const bool state) {
            countdown_from_prev_ = state;
        }

        /// True if the report includes the delta since start().
        [[nodiscard]] bool is_count_from_start() const {
            return countdown_from_start_;
        }

        /// Sets whether the report includes the delta since start().
        void set_countdown_from_start(const bool state) {
            countdown_from_start_ = state;
        }

        /// name labels the run in print()'s report; flags_cnt_reserve pre-sizes the
        /// flag vector to avoid reallocation while timing.
        explicit PrintingStopwatch(std::string name = "", const std::size_t flags_cnt_reserve = 30)
            : running_name_(std::move(name)) {
            flags_.reserve(flags_cnt_reserve);
        }
        /// This type's display name, "Stopwatch".
        static constexpr const char* name() {
            return "Stopwatch";
        }

        /// Prints the report (see print()).
        ~PrintingStopwatch() noexcept {
            print();
        }

        /// Renames the run, clears flags, and records a new start time.
        void start(std::string name) noexcept {
            running_name_ = std::move(name);
            start_time_   = now();
            flags_.clear();
        }
        /// Clears flags and records a new start time, keeping the current name.
        void start() noexcept {
            start_time_ = now();
            flags_.clear();
        }
        /// Records a final "Finish" flag, then prints the report (see print()).
        void finish() noexcept {
            flag("Finish");
            print();
        }

        /// Prints every flag's name and configured deltas (since-previous and/or
        /// since-start, per set_countdown_from_prev()/set_countdown_from_start()) to
        /// std::cout, then clears the flags. No-op if no flags are recorded.
        void print() noexcept {
            if (flags_.empty()) {
                return;
            }

            std::ostringstream stream;
            stream << "\nRunning: " << running_name_ << '\n' << "Stopwatch times (in " << time_unit_name() << "):\n";
            auto previous = start_time_;
            for (size_t i = 0; i < flags_.size(); ++i) {
                if (flags_[i].name_.empty()) {
                    stream << "Flag " << i + 1;
                } else {
                    stream << flags_[i].name_;
                }
                if (countdown_from_prev_) {
                    stream << "  |  " << std::chrono::duration_cast<T>(flags_[i].point_ - previous).count()
                           << time_unit_name_short();
                }
                if (countdown_from_start_) {
                    stream << "  |  " << std::chrono::duration_cast<T>(flags_[i].point_ - start_time_).count()
                           << time_unit_name_short();
                }
                stream << "\n";
                previous = flags_[i].point_;
            }
            stream << std::endl;
            std::cout << stream.str();
            flags_.clear();
        }

        /// Equivalent to start(): clears flags and records a new start time.
        void reset() noexcept {
            start();
        }

        /// Adds an unnamed flag (postfix form).
        PrintingStopwatch& operator++(int) {
            flag("");
            return *this;
        }

        /// Adds an unnamed flag (prefix form).
        PrintingStopwatch& operator++() {
            flag("");
            return *this;
        }

        /// Removes the last flag, if any (postfix form).
        PrintingStopwatch& operator--(int) {
            if (!flags_.empty()) {
                flags_.pop_back();
            }
            return *this;
        }

        /// Removes the last flag, if any (prefix form).
        PrintingStopwatch& operator--() {
            if (!flags_.empty()) {
                flags_.pop_back();
            }
            return *this;
        }

        /// Records a flag with the given name; name must be std::string or convertible to it.
        template <typename string_mv>
        std::enable_if_t<std::is_same_v<std::remove_cvref_t<string_mv>, std::string> ||
                             std::is_convertible_v<string_mv, std::string>,
                         void>
        flag(string_mv&& name) {
            flags_.emplace_back(std::forward<string_mv>(name), now());
        }

        // Overload -- operator to remove the last flag

    private:
        bool countdown_from_prev_  = true;
        bool countdown_from_start_ = true;
        std::string running_name_;

        /// A named timestamp recorded by flag().
        struct Flag {
            std::string name_;
            std::chrono::time_point<std::chrono::high_resolution_clock> point_;

            Flag(std::string&& name, const std::chrono::time_point<std::chrono::high_resolution_clock> point)
                : name_(std::move(name)),
                  point_(point) {
            }

            Flag(const std::string& name, const std::chrono::time_point<std::chrono::high_resolution_clock> point)
                : name_(name),
                  point_(point) {
            }
        };

        /// The current time.
        static constexpr auto now() noexcept {
            return std::chrono::high_resolution_clock::now();
        }

        /// T's unit name for the report header (e.g. "milliseconds").
        [[nodiscard]] static constexpr const char* time_unit_name() {
            if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
                return "milliseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
                return "microseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
                return "nanoseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::seconds>) {
                return "seconds";
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for time");
            }
            return {};
        }

        /// T's short unit suffix for per-flag deltas (e.g. "mls" for milliseconds).
        [[nodiscard]] static constexpr const char* time_unit_name_short() {
            if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
                return "mls";
            } else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
                return "mcs";
            } else if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
                return "ns";
            } else if constexpr (std::is_same_v<T, std::chrono::seconds>) {
                return "s";
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for time");
            }
            return {};
        }

        std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
        std::vector<Flag> flags_;
    };
}  // namespace menagerie::chrono
