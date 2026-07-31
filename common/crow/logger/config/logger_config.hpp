#pragma once

#include <chrono>
#include <menagerie/serialization>
namespace menagerie::crow {

    /// Logger construction options: ring buffer size, internal-pool size, and the
    /// consumer's wait strategy. Build via LoggerConfig::Builder, which validates
    /// on finalize().
    class LoggerConfig final : public serialization::ConfigInterface<LoggerConfig, Json::Value> {
    public:
        /// How the consumer thread waits for new events when the ring buffer is empty.
        enum class WaitStrategy {
            BusySpin,  ///< Lowest latency; spins the consumer thread continuously.
            Yielding,  ///< Balanced; yields the consumer thread between checks.
            Blocking   ///< Lowest CPU; blocks the consumer thread between checks.
        };

        /// Named ring_buffer_size presets (must still be a power of two).
        struct BufferCapacity {
            static constexpr std::size_t Small  = 1024;    ///< Low-throughput / memory-constrained.
            static constexpr std::size_t Medium = 8192;    ///< Default: fits most workloads.
            static constexpr std::size_t Large  = 65536;   ///< High-throughput producers.
            static constexpr std::size_t Huge   = 131072;  ///< Bursty, very high-throughput producers.
        };

        /// Full constructor (escape hatch); prefer LoggerConfig::Builder for named,
        /// optional-with-defaults construction.
        constexpr LoggerConfig(const std::size_t ring_buffer_size,
                               const std::size_t pool_size,
                               const WaitStrategy wait_strategy,
                               const std::chrono::milliseconds health_check_interval)
            : ring_buffer_size_{ring_buffer_size},
              pool_size_{pool_size},
              wait_strategy_{wait_strategy},
              health_check_interval_{health_check_interval} {
        }

        /// @throw std::invalid_argument if ring_buffer_size() is not a power of two.
        constexpr void validate() const override {
            if (std::popcount(ring_buffer_size_) != 1) {
                throw std::invalid_argument("Ring buffer size must be a power of 2");
            }
        }

        /// Returns the configured ring buffer capacity (power of two).
        [[nodiscard]] constexpr std::size_t ring_buffer_size() const noexcept {
            return ring_buffer_size_;
        }

        /// Returns the configured consumer wait strategy.
        [[nodiscard]] constexpr WaitStrategy wait_strategy() const noexcept {
            return wait_strategy_;
        }

        /// Returns the configured internal thread pool size.
        [[nodiscard]] constexpr std::size_t pool_size() const noexcept {
            return pool_size_;
        }

        /// Returns how often the janitor sweeps sinks; zero disables the janitor thread.
        [[nodiscard]] constexpr std::chrono::milliseconds health_check_interval() const noexcept {
            return health_check_interval_;
        }

        /// Field list consumed by serialization::ConfigInterface for JSON (de)serialization.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&LoggerConfig::ring_buffer_size_, "ring_buffer_size">{},
                serialization::Field<&LoggerConfig::pool_size_, "pool_size">{},
                serialization::Field<&LoggerConfig::wait_strategy_, "wait_strategy">{},
                serialization::Field<&LoggerConfig::health_check_interval_, "health_check_interval">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr LoggerConfig() = default;

        std::size_t ring_buffer_size_ = BufferCapacity::Medium;
        /// Each sink is serialized on its own strand, so parallelism beyond the sink
        /// count is unusable; two threads keep a slow sink from stalling the others.
        std::size_t pool_size_        = 2;
        WaitStrategy wait_strategy_   = WaitStrategy::Yielding;
        std::chrono::milliseconds health_check_interval_{1000};
    };

    /// Fluent builder for LoggerConfig: chained setters, then finalize() validates
    /// and returns the built config.
    class LoggerConfig::Builder {
    public:
        Builder() = default;
        /// Seeds the builder from an existing config, copying its current values.
        explicit Builder(const LoggerConfig& existing)
            : config_{existing} {
        }
        /// Seeds the builder from an existing config, moving its current values.
        explicit Builder(LoggerConfig&& existing)
            : config_{std::move(existing)} {
        }

        /// Sets the ring buffer capacity; must be a power of two (checked in finalize()).
        template <typename Self>
        constexpr auto&& ring_buffer_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.ring_buffer_size_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the consumer wait strategy.
        template <typename Self>
        constexpr auto&& wait_strategy(this Self&& self, const WaitStrategy value) noexcept {
            self.config_.wait_strategy_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the internal thread pool size.
        template <typename Self>
        constexpr auto&& pool_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.pool_size_ = value;
            return std::forward<Self>(self);
        }

        /// Sets how often the janitor sweeps sinks. Zero disables it: the gate then only
        /// refreshes on add_sink()/remove_sink()/sweep(), and nothing recovers on its own.
        template <typename Self>
        constexpr auto&& health_check_interval(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.health_check_interval_ = value;
            return std::forward<Self>(self);
        }

        /// Validates and returns the built config.
        /// @throw std::invalid_argument if ring_buffer_size() is not a power of two.
        [[nodiscard]] LoggerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class LoggerConfig;
        friend class ConfigInterface;
        LoggerConfig config_;
    };

}  // namespace menagerie::crow
