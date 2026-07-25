#pragma once

#include <menagerie/chameleon>
#include <mutex>

#include "config/console_sink_config.hpp"

namespace menagerie::crow {


    /**
     * @brief Console sink with ANSI color support
     *
     * @tparam EntryType Defines which metadata to include (e.g., DetailedEntry, LightEntry)
     *
     * Features:
     * - ANSI color-coded output based on log level
     * - Configurable threshold filtering
     * - Thread-safe writing
     * - Optional per-entry flushing
     *
     * Color scheme:
     * - TRC/DBG: Cyan
     * - INF: Green
     * - WRN: Yellow
     * - ERR: Red
     * - FAT: Bold red
     *
     */
    template <detail::EntryConcept EntryType>
    class ConsoleSink final : public Sink {
    public:
        /// Constructs from a ConsoleSinkConfig (or anything constructible into one).
        template <typename ConsoleSinkConfigTp = ConsoleSinkConfig>
            requires std::constructible_from<ConsoleSinkConfig, ConsoleSinkConfigTp>
        explicit ConsoleSink(ConsoleSinkConfigTp&& cfg) noexcept
            : config_{std::forward<ConsoleSinkConfigTp>(cfg)} {
        }

        void process(const LogEvent& event) override {
            if (!should_log(event.level, event.prefix.view())) {
                return;
            }

            auto entry = make_entry_from_event<EntryType>(event);
            entry.format_into(format_buffer_);

            std::lock_guard lock{mutex_};

            if (config_.enable_colors()) {
                *config_.output() << colorize_by_level(format_buffer_, entry.level());
            } else {
                *config_.output() << format_buffer_;
            }

            if (config_.flush_each_entry()) {
                config_.output()->flush();
            }
        }

        void flush() override {
            std::lock_guard lock{mutex_};
            config_.output()->flush();
        }

        [[nodiscard]] bool should_log(LogLevel lvl, const std::string_view prefix) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.threshold()) &&
                   config_.prefix_filter().accepts(prefix);
        }

        /// Replaces the sink's config at runtime.
        void set_config(ConsoleSinkConfig cfg) noexcept {
            config_ = std::move(cfg);
        }

        /// The sink's current config.
        [[nodiscard]] constexpr const ConsoleSinkConfig& config() const noexcept {
            return config_;
        }

    private:
        ConsoleSinkConfig config_;
        mutable std::mutex mutex_;
        std::string format_buffer_;

        /// Wraps text in the ANSI color associated with lvl (Trace/Debug: cyan, Info:
        /// green, Warning: yellow, Error: red, Fatal: bold red).
        [[nodiscard]] static std::string colorize_by_level(const std::string_view text, const LogLevel lvl) {
            using namespace menagerie::chameleon::colors;

            switch (lvl) {
                case LogLevel::Trace:
                case LogLevel::Debug:
                    return make_cyan(text);
                case LogLevel::Info:
                    return make_green(text);
                case LogLevel::Warning:
                    return make_yellow(text);
                case LogLevel::Error:
                    return make_red(text);
                case LogLevel::Fatal:
                    return make_bold_red(text);
                default:
                    return std::string{text};
            }
        }
    };
}  // namespace menagerie::crow
