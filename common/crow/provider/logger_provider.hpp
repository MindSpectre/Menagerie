#pragma once

#include <memory>
#include <string_view>

#include "console_sink.hpp"
#include "file_sink.hpp"
#include "logger.hpp"

namespace menagerie::crow {
    /**
     * @brief Logger provider - wraps logger instance
     *
     * Allows dependency injection and testing.
     *
     * The @p prefix_ is a class-name / subsystem label rendered into log output
     * and usable by per-sink `PrefixFilter` for allow/deny routing.
     *
     * Prefix overflow: values longer than `PrefixNameStorage` capacity are
     * silently truncated at runtime (see `PrefixNameStorage` docs). Prefer
     * `SCROLL_COMPONENT_PREFIX` for literal names -- it catches overflow at
     * compile time.
     *
     * Thread safety: @ref set_prefix is NOT thread-safe. Callers must set the
     * prefix during construction before the object is visible to other threads.
     * Concurrent `set_prefix` with active logging is UB (data race).
     */
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        /// Single 2-arg constructor with default prefix -- the 1-arg
        /// `LoggerProvider{logger}` form stays valid (prefix defaults empty).
        /// `explicit` prevents implicit conversion from `shared_ptr<Logger>`
        /// alone and avoids ambiguous overload resolution we'd hit if we
        /// also declared a separate 1-arg constructor.
        constexpr explicit LoggerProvider(std::shared_ptr<Logger> logger, const std::string_view prefix = {})
            : logger_{std::move(logger)} {
            prefix_.assign(prefix);
        }

        /// The owned logger, or nullptr if none has been set.
        [[nodiscard]] Logger* get_logger() noexcept {
            return logger_.get();
        }

        /// @overload
        [[nodiscard]] const Logger* get_logger() const noexcept {
            return logger_.get();
        }

        /// The class-name / subsystem label rendered into this provider's log output.
        [[nodiscard]] constexpr const PrefixNameStorage& prefix() const noexcept {
            return prefix_;
        }

        /// Replaces the owned logger.
        void set_logger(std::shared_ptr<Logger> logger) noexcept {
            logger_ = std::move(logger);
        }

        /// @note NOT thread-safe -- see class docs.
        /// @note Oversized @p prefix is silently truncated at runtime.
        constexpr void set_prefix(const std::string_view prefix) noexcept {
            prefix_.assign(prefix);
        }

    private:
        std::shared_ptr<Logger> logger_;
        PrefixNameStorage prefix_;
    };

    /**
     * @brief Global logger manager for component logging
     *
     * Provides static singleton access to logger instance.
     * Used by COMPONENT_LOG_* macros.
     */
    class ComponentLoggerManager {
    public:
        /// Returns the shared component logger, lazily calling initialize() on first use.
        static Logger* get() {
            // Lazy initialization on first use
            if (!logger_) {
                initialize();
            }
            return logger_.get();
        }

        /// Sets up the shared component logger: reuses a Logger registered with
        /// spider if one is available, else falls back to an owned Logger with a
        /// default colored-console sink.
        static void initialize();


        /// Allows manual override for testing or custom configuration.
        static void set_logger(std::shared_ptr<Logger> logger) {
            logger_ = std::move(logger);
        }

    private:
        static inline std::shared_ptr<Logger> logger_ = nullptr;
    };
    // todo: possibly make default file sink logger
}  // namespace menagerie::crow
