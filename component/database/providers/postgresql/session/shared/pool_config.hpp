#pragma once

#include <chrono>
#include <cstddef>
#include <menagerie/serialization>
#include <stdexcept>
#include <string>


namespace menagerie::db::postgres {

    /**
     * @brief Configuration for the connection pool
     *
     * Controls pool sizing, connection timeouts, health check intervals,
     * and cleanup behavior. Capacity must be a power of 2 for efficient
     * ring buffer masking.
     *
     * Usage:
     *   auto cfg = PoolConfig::Builder{}
     *       .capacity(16)
     *       .min_connections(2)
     *       .connect_timeout(std::chrono::seconds{5})
     *       .finalize();
     */
    class PoolConfig final : public serialization::ConfigInterface<PoolConfig, Json::Value> {
    public:
        // -------- ConfigInterface Implementation --------

        /**
         * @brief Validate pool sizing invariants
         * @throw std::invalid_argument if capacity is zero, if capacity is not a power
         *        of 2, if min_connections exceeds capacity, or if connect_timeout is
         *        not positive.
         */
        constexpr void validate() const override {
            if (capacity_ == 0) {
                throw std::invalid_argument("Pool capacity must be greater than 0");
            }
            if ((capacity_ & (capacity_ - 1)) != 0) {
                throw std::invalid_argument("Pool capacity must be a power of 2");
            }
            if (min_connections_ > capacity_) {
                throw std::invalid_argument("min_connections cannot exceed capacity");
            }
            if (connect_timeout_.count() <= 0) {
                throw std::invalid_argument("connect_timeout must be positive");
            }
        }

        // -------- Getters --------

        /// Ring buffer capacity (slot count); always a power of 2.
        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return capacity_;
        }
        /// Number of slots pre-warmed with a live connection when the pool is constructed.
        [[nodiscard]] constexpr std::size_t min_connections() const noexcept {
            return min_connections_;
        }
        /// Configured timeout for establishing a new connection.
        [[nodiscard]] constexpr std::chrono::seconds connect_timeout() const noexcept {
            return connect_timeout_;
        }
        /// Configured maximum idle duration for a FREE slot.
        [[nodiscard]] constexpr std::chrono::seconds idle_timeout() const noexcept {
            return idle_timeout_;
        }
        /// Interval between PoolJanitor sweeps of the pool.
        [[nodiscard]] constexpr std::chrono::seconds health_check_interval() const noexcept {
            return health_check_interval_;
        }
        /// Configured maximum connection age before it is recycled.
        [[nodiscard]] constexpr std::chrono::seconds max_lifetime() const noexcept {
            return max_lifetime_;
        }
        /// Default cleanup SQL for this pool's connections, or an empty string if none configured.
        [[nodiscard]] constexpr const char* cleanup_sql() const noexcept {
            return cleanup_sql_.c_str();
        }

        // -------- Factory Methods (defined after Builder) --------

        /// Small pool for tests and low-traffic services: capacity 2, min_connections 1.
        [[nodiscard]] static PoolConfig minimal();
        /// Default-sized pool: capacity 16, min_connections 2, other fields at their defaults.
        [[nodiscard]] static PoolConfig standard();
        /// Large pool for high-throughput services: capacity 64, min_connections 8, faster health checks.
        [[nodiscard]] static PoolConfig high_performance();

        // -------- Field Descriptors --------

        /// Field descriptor tuple consumed by the serialization framework's auto-serialize support.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&PoolConfig::capacity_, "capacity">{},
                serialization::Field<&PoolConfig::min_connections_, "min_connections">{},
                serialization::Field<&PoolConfig::connect_timeout_, "connect_timeout">{},
                serialization::Field<&PoolConfig::idle_timeout_, "idle_timeout">{},
                serialization::Field<&PoolConfig::health_check_interval_, "health_check_interval">{},
                serialization::Field<&PoolConfig::max_lifetime_, "max_lifetime">{},
                serialization::Field<&PoolConfig::cleanup_sql_, "cleanup_sql">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr PoolConfig() = default;

        std::size_t capacity_        = 16;
        std::size_t min_connections_ = 2;

        std::chrono::seconds connect_timeout_       = std::chrono::seconds{10};
        std::chrono::seconds idle_timeout_          = std::chrono::seconds{300};
        std::chrono::seconds health_check_interval_ = std::chrono::seconds{30};
        std::chrono::seconds max_lifetime_          = std::chrono::seconds{3600};

        std::string cleanup_sql_;
    };

    /**
     * @brief Fluent builder for PoolConfig
     *
     * Chainable setters return *this (forwarded through a deducing-this
     * "this Self&&" parameter) so calls compose into a single expression ending
     * in finalize(). Unset fields keep PoolConfig's own defaults.
     */
    class PoolConfig::Builder {
    public:
        /// Starts from PoolConfig's built-in defaults.
        Builder() = default;
        /// Starts pre-populated by copying an existing PoolConfig's fields.
        explicit Builder(const PoolConfig& existing)
            : config_{existing} {
        }
        /// Starts pre-populated by moving an existing PoolConfig's fields.
        explicit Builder(PoolConfig&& existing)
            : config_{std::move(existing)} {
        }

        /// Sets the ring buffer capacity; must be a nonzero power of 2 (checked by finalize()).
        template <typename Self>
        constexpr auto&& capacity(this Self&& self, std::size_t value) noexcept {
            self.config_.capacity_ = value;
            return std::forward<Self>(self);
        }

        /// Sets how many slots are pre-warmed with a live connection at construction; must not exceed capacity().
        template <typename Self>
        constexpr auto&& min_connections(this Self&& self, std::size_t value) noexcept {
            self.config_.min_connections_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the connection-attempt timeout; must be positive (checked by finalize()).
        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the maximum idle duration for a FREE slot.
        template <typename Self>
        constexpr auto&& idle_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.idle_timeout_ = value;
            return std::forward<Self>(self);
        }

        /// Sets how often the background janitor sweeps the pool.
        template <typename Self>
        constexpr auto&& health_check_interval(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.health_check_interval_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the maximum connection age before it is recycled.
        template <typename Self>
        constexpr auto&& max_lifetime(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.max_lifetime_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the default cleanup SQL for this pool's connections.
        template <typename Self>
        constexpr auto&& cleanup_sql(this Self&& self, std::string value) noexcept {
            self.config_.cleanup_sql_ = std::move(value);
            return std::forward<Self>(self);
        }

        /**
         * @brief Validate and finalize the builder into a PoolConfig
         * @throw std::invalid_argument if capacity is zero, if capacity is not a power
         *        of 2, if min_connections exceeds capacity, or if connect_timeout is
         *        not positive.
         */
        [[nodiscard]] PoolConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class PoolConfig;
        friend class ConfigInterface;
        PoolConfig config_;
    };

    // -------- PoolConfig Factory Method Definitions --------

    inline PoolConfig PoolConfig::minimal() {
        return Builder{}.capacity(2).min_connections(1).finalize();
    }

    inline PoolConfig PoolConfig::standard() {
        return Builder{}.finalize();
    }

    inline PoolConfig PoolConfig::high_performance() {
        return Builder{}.capacity(64).min_connections(8).health_check_interval(std::chrono::seconds{15}).finalize();
    }

}  // namespace menagerie::db::postgres
