#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <menagerie/crow>

namespace crow_test {
    /// Counts what the Logger actually dispatched to it.
    class CountingSink final : public menagerie::crow::Sink {
    public:
        explicit CountingSink(const menagerie::crow::LogLevel threshold)
            : threshold_{threshold} {
            publish_threshold(threshold);
        }

        void process(const menagerie::crow::LogEvent&) noexcept override {
            events_.fetch_add(1, std::memory_order_relaxed);
        }

        void process_batch(const std::shared_ptr<std::vector<menagerie::crow::LogEvent>>& batch) noexcept override {
            batches_.fetch_add(1, std::memory_order_relaxed);
            events_.fetch_add(batch->size(), std::memory_order_relaxed);
        }

        void flush() noexcept override {
        }

        [[nodiscard]] bool should_log(const menagerie::crow::LogLevel lvl, std::string_view) const noexcept override {
            return static_cast<std::uint8_t>(lvl) >= static_cast<std::uint8_t>(threshold_);
        }

        [[nodiscard]] std::uint64_t events() const noexcept {
            return events_.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t batches() const noexcept {
            return batches_.load(std::memory_order_relaxed);
        }

    private:
        menagerie::crow::LogLevel threshold_;
        std::atomic<std::uint64_t> events_{0};
        std::atomic<std::uint64_t> batches_{0};
    };
}  // namespace crow_test
