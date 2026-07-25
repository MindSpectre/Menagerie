#pragma once

#include <iostream>
#include <menagerie/serialization>

#include "detail/prefix_filter.hpp"
#include "sink_interface.hpp"

namespace menagerie::crow {

    /// ConsoleSink construction options: severity threshold, ANSI color/flush
    /// behavior, output stream, and prefix filtering. Build via
    /// ConsoleSinkConfig::Builder.
    class ConsoleSinkConfig final : public serialization::ConfigInterface<ConsoleSinkConfig, Json::Value> {
    public:
        /// Full constructor (escape hatch); prefer ConsoleSinkConfig::Builder for named,
        /// optional-with-defaults construction.
        constexpr ConsoleSinkConfig(const LogLevel threshold,
                                    const bool enable_colors,
                                    const bool flush_each_entry,
                                    std::ostream* const output,
                                    PrefixFilter prefix_filter = {}) noexcept
            : threshold_{threshold},
              enable_colors_{enable_colors},
              flush_each_entry_{flush_each_entry},
              output_{output},
              prefix_filter_{std::move(prefix_filter)} {
        }

        /// No invariants to check; always valid.
        constexpr void validate() const override {
            // always valid
        }

        /// Returns the minimum level this sink logs.
        [[nodiscard]] constexpr LogLevel threshold() const noexcept {
            return threshold_;
        }
        /// Returns whether output is ANSI color-coded by level.
        [[nodiscard]] constexpr bool enable_colors() const noexcept {
            return enable_colors_;
        }
        /// Returns whether output is flushed after every entry.
        [[nodiscard]] constexpr bool flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        /// Returns the stream entries are written to.
        [[nodiscard]] constexpr std::ostream* output() const noexcept {
            return output_;
        }
        /// Returns the configured prefix allow/deny filter.
        [[nodiscard]] constexpr const PrefixFilter& prefix_filter() const noexcept {
            return prefix_filter_;
        }

        /// Field list consumed by serialization::ConfigInterface for JSON (de)serialization.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ConsoleSinkConfig::threshold_, "threshold">{},
                serialization::Field<&ConsoleSinkConfig::enable_colors_, "enable_colors">{},
                serialization::Field<&ConsoleSinkConfig::flush_each_entry_, "flush_each_entry">{},
                serialization::Field<&ConsoleSinkConfig::output_, "output", serialization::FieldPolicy::Excluded>{},
                serialization::
                    Field<&ConsoleSinkConfig::prefix_filter_, "prefix_filter", serialization::FieldPolicy::Excluded>{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr ConsoleSinkConfig() = default;

        LogLevel threshold_    = LogLevel::Debug;
        bool enable_colors_    = true;
        bool flush_each_entry_ = false;
        std::ostream* output_  = &std::cout;
        PrefixFilter prefix_filter_{};
    };

    /// Fluent builder for ConsoleSinkConfig: chained setters, then finalize()
    /// validates and returns the built config.
    class ConsoleSinkConfig::Builder {
    public:
        Builder() = default;

        /// Seeds the builder from an existing config, copying or moving its values.
        template <typename ConsoleSinkConfigTp>
            requires std::is_same_v<std::remove_cvref_t<ConsoleSinkConfigTp>, ConsoleSinkConfig>
        explicit Builder(ConsoleSinkConfigTp&& existing)
            : config_{std::forward<ConsoleSinkConfigTp>(existing)} {
        }

        /// Sets the minimum level this sink logs.
        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel value) noexcept {
            self.config_.threshold_ = value;
            return std::forward<Self>(self);
        }

        /// Sets whether output is ANSI color-coded by level.
        template <typename Self>
        constexpr auto&& enable_colors(this Self&& self, const bool value) noexcept {
            self.config_.enable_colors_ = value;
            return std::forward<Self>(self);
        }

        /// Sets whether output is flushed after every entry.
        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool value) noexcept {
            self.config_.flush_each_entry_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the stream entries are written to.
        template <typename Self>
        constexpr auto&& output(this Self&& self, std::ostream* value) noexcept {
            self.config_.output_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the prefix allow/deny filter.
        template <typename Self>
        constexpr auto&& prefix_filter(this Self&& self, PrefixFilter value) noexcept {
            self.config_.prefix_filter_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Validates and returns the built config.
        [[nodiscard]] constexpr ConsoleSinkConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ConsoleSinkConfig;
        friend class ConfigInterface;
        ConsoleSinkConfig config_;
    };

}  // namespace menagerie::crow
