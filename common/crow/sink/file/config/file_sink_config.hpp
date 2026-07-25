#pragma once

#include <filesystem>
#include <menagerie/beavers>
#include <menagerie/chrono>
#include <menagerie/serialization>

#include "detail/prefix_filter.hpp"
#include "sink_interface.hpp"

namespace menagerie::crow {

    /// FileSink construction options: severity threshold, target file, timestamped
    /// naming, rotation, and prefix filtering. Build via FileSinkConfig::Builder.
    class FileSinkConfig final : public serialization::ConfigInterface<FileSinkConfig, Json::Value> {
    public:
        /// Full constructor (escape hatch); prefer FileSinkConfig::Builder for named,
        /// optional-with-defaults construction.
        constexpr FileSinkConfig(const LogLevel threshold,
                                 std::filesystem::path file,
                                 const bool add_time_to_filename,
                                 std::string time_format_in_file_name,
                                 const bool rotate_file,
                                 const std::uint64_t max_file_size,
                                 const bool flush_each_entry,
                                 PrefixFilter prefix_filter = {}) noexcept
            : threshold_{threshold},
              file_{std::move(file)},
              add_time_to_filename_{add_time_to_filename},
              time_format_in_file_name_{std::move(time_format_in_file_name)},
              rotate_file_{rotate_file},
              max_file_size_{max_file_size},
              flush_each_entry_{flush_each_entry},
              prefix_filter_{std::move(prefix_filter)} {
        }

        /// @throw std::invalid_argument if rotate_file() is set with max_file_size() == 0,
        ///        file() is empty, add_time_to_filename() is set with an empty
        ///        time_format_in_file_name(), or rotate_file() is set without
        ///        add_time_to_filename() (rotation needs a changing filename).
        constexpr void validate() const override {
            if (rotate_file_ && max_file_size_ == 0) {
                throw std::invalid_argument("max_file_size must be greater than 0");
            }
            if (file_.empty()) {
                throw std::invalid_argument("file path must be specified");
            }
            if (add_time_to_filename_ && time_format_in_file_name_.empty()) {
                throw std::invalid_argument("time format must be specified");
            }
            if (rotate_file_ && !add_time_to_filename_) {
                throw std::invalid_argument("rotation is enabled, but the dynamic filename is disabled");
            }
        }

        /// Returns the minimum level this sink logs.
        [[nodiscard]] constexpr LogLevel threshold() const noexcept {
            return threshold_;
        }
        /// Returns the target log file path.
        [[nodiscard]] constexpr const std::filesystem::path& file() const noexcept {
            return file_;
        }
        /// Returns whether a timestamp is appended to file() when writing.
        [[nodiscard]] constexpr bool add_time_to_filename() const noexcept {
            return add_time_to_filename_;
        }
        /// Returns the strftime-style format used when add_time_to_filename() is set.
        [[nodiscard]] constexpr const std::string& time_format_in_file_name() const noexcept {
            return time_format_in_file_name_;
        }
        /// Returns the size threshold (bytes) that triggers rotation when rotate_file() is set.
        [[nodiscard]] constexpr std::uint64_t max_file_size() const noexcept {
            return max_file_size_;
        }
        /// Returns whether the file is flushed after every entry.
        [[nodiscard]] constexpr bool flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        /// Returns whether the log file rotates once it exceeds max_file_size().
        [[nodiscard]] constexpr bool rotate_file() const noexcept {
            return rotate_file_;
        }
        /// Returns the configured prefix allow/deny filter.
        [[nodiscard]] const PrefixFilter& prefix_filter() const noexcept {
            return prefix_filter_;
        }

        /// Field list consumed by serialization::ConfigInterface for JSON (de)serialization.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&FileSinkConfig::threshold_, "threshold">{},
                serialization::Field<&FileSinkConfig::file_, "file">{},
                serialization::Field<&FileSinkConfig::add_time_to_filename_, "add_time_to_filename">{},
                serialization::Field<&FileSinkConfig::time_format_in_file_name_, "time_format_in_file_name">{},
                serialization::Field<&FileSinkConfig::rotate_file_, "rotate_file">{},
                serialization::Field<&FileSinkConfig::max_file_size_, "max_file_size">{},
                serialization::Field<&FileSinkConfig::flush_each_entry_, "flush_each_entry">{},
                serialization::
                    Field<&FileSinkConfig::prefix_filter_, "prefix_filter", serialization::FieldPolicy::Excluded>{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr FileSinkConfig() = default;

        LogLevel threshold_ = LogLevel::Debug;
        std::filesystem::path file_;
        bool add_time_to_filename_            = true;
        std::string time_format_in_file_name_ = chrono::clock_formats::iso8601;

        bool rotate_file_            = true;
        std::uint64_t max_file_size_ = beavers::literals::operator""_mb(100);
        bool flush_each_entry_       = false;
        PrefixFilter prefix_filter_{};
    };

    /// Fluent builder for FileSinkConfig: chained setters, then finalize()
    /// validates and returns the built config.
    class FileSinkConfig::Builder {
    public:
        Builder() = default;

        /// Seeds the builder from an existing config, copying or moving its values.
        template <typename FileSinkConfigTp>
            requires std::is_same_v<std::remove_cvref_t<FileSinkConfigTp>, FileSinkConfig>
        explicit Builder(FileSinkConfigTp&& existing)
            : config_{std::forward<FileSinkConfigTp>(existing)} {
        }
        /// Sets the minimum level this sink logs.
        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel value) noexcept {
            self.config_.threshold_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the target log file path.
        template <typename Self>
        constexpr auto&& file(this Self&& self, std::filesystem::path value) noexcept {
            self.config_.file_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets whether a timestamp is appended to the file name when writing.
        template <typename Self>
        constexpr auto&& add_time_to_filename(this Self&& self, const bool value) noexcept {
            self.config_.add_time_to_filename_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the strftime-style format used when add_time_to_filename() is set.
        template <typename Self>
        constexpr auto&& time_format_in_file_name(this Self&& self, std::string value) noexcept {
            self.config_.time_format_in_file_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the size threshold (bytes) that triggers rotation when rotate_file() is set.
        template <typename Self>
        constexpr auto&& max_file_size(this Self&& self, const std::uint64_t value) noexcept {
            self.config_.max_file_size_ = value;
            return std::forward<Self>(self);
        }

        /// Sets whether the file is flushed after every entry.
        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool value) noexcept {
            self.config_.flush_each_entry_ = value;
            return std::forward<Self>(self);
        }

        /// Sets rotate_file() (the getter is named rotate_file, not rotation).
        template <typename Self>
        constexpr auto&& rotation(this Self&& self, const bool value) noexcept {
            self.config_.rotate_file_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the prefix allow/deny filter.
        template <typename Self>
        constexpr auto&& prefix_filter(this Self&& self, PrefixFilter value) noexcept {
            self.config_.prefix_filter_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Validates and returns the built config.
        /// @throw std::invalid_argument on invalid combinations; see FileSinkConfig::validate().
        [[nodiscard]] constexpr FileSinkConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class FileSinkConfig;
        friend class ConfigInterface;
        FileSinkConfig config_;
    };

}  // namespace menagerie::crow
