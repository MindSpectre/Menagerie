#pragma once

#include <chrono>
#include <menagerie/serialization>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace menagerie::http {

    /**
     * @brief Per-phase HTTP timeout set, JSON-loadable.
     *
     * Field names carry the unit (header_ms/body_ms/idle_ms): JSON values are
     * plain integers interpreted as milliseconds. attach_default_listeners
     * maps these onto Http11Config's per-phase timeouts.
     *
     * idle_ms bounds the keep-alive wait for the next request; header_ms and
     * body_ms bound a message already mid-arrival (see http11_config.hpp).
     */
    class Timeouts final : public serialization::ConfigInterface<Timeouts, Json::Value> {
    public:
        /// Constructs from explicit per-phase durations; does not validate.
        constexpr Timeouts(const std::chrono::milliseconds header,
                           const std::chrono::milliseconds body,
                           const std::chrono::milliseconds idle) noexcept
            : header_{header},
              body_{body},
              idle_{idle} {
        }

        constexpr void validate() const override {
            if (header_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.header_ms must be positive");
            }
            if (body_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.body_ms must be positive");
            }
            if (idle_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.idle_ms must be positive");
            }
        }

        /// The configured header-read timeout.
        [[nodiscard]] constexpr std::chrono::milliseconds header() const noexcept {
            return header_;
        }
        /// The configured body-read timeout.
        [[nodiscard]] constexpr std::chrono::milliseconds body() const noexcept {
            return body_;
        }
        /// The configured keep-alive idle timeout.
        [[nodiscard]] constexpr std::chrono::milliseconds idle() const noexcept {
            return idle_;
        }

        /// Field descriptors consumed by the JSON (de)serialization machinery.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&Timeouts::header_, "header_ms">{},
                serialization::Field<&Timeouts::body_, "body_ms">{},
                serialization::Field<&Timeouts::idle_, "idle_ms">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr Timeouts() = default;

        std::chrono::milliseconds header_ = std::chrono::seconds{10};
        std::chrono::milliseconds body_   = std::chrono::seconds{30};
        std::chrono::milliseconds idle_   = std::chrono::seconds{60};
    };

    /**
     * @brief Fluent builder for Timeouts.
     *
     * Chainable setters (`this Self&&`) work on both lvalue and rvalue
     * builders; finalize() validates and returns the built config.
     */
    class Timeouts::Builder {
    public:
        Builder() = default;

        /// Sets the header-read timeout.
        template <typename Self>
        constexpr auto&& header(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.header_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the body-read timeout.
        template <typename Self>
        constexpr auto&& body(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.body_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the keep-alive idle timeout.
        template <typename Self>
        constexpr auto&& idle(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.idle_ = value;
            return std::forward<Self>(self);
        }

        /// Validates and returns the built Timeouts.
        [[nodiscard]] constexpr Timeouts finalize() && {
            config_.validate();
            return config_;
        }

    private:
        friend class Timeouts;
        friend class ConfigInterface;
        Timeouts config_;
    };

}  // namespace menagerie::http
