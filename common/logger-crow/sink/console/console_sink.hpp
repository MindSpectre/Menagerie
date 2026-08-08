#pragma once

#include <exception>
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
            publish_threshold(config_.threshold());
        }

        void process(const LogEvent& event) noexcept override {
            if (!should_log(event.level, event.prefix.view())) {
                return;
            }

            std::lock_guard lock{mutex_};
            try {
                auto entry = make_entry_from_event<EntryType>(event);
                entry.format_into(format_buffer_);

                if (config_.enable_colors()) {
                    *config_.output() << colorize_by_level(format_buffer_, entry.level());
                } else {
                    *config_.output() << format_buffer_;
                }
                if (config_.flush_each_entry()) {
                    config_.output()->flush();
                }
            } catch (const std::exception& e) {
                mark_degraded(e.what());
                return;
            } catch (...) {
                mark_degraded("unknown exception while writing");
                return;
            }

            if (!config_.output()->good()) {
                mark_degraded("output stream is not writable");
            }
        }

        void flush() noexcept override {
            std::lock_guard lock{mutex_};
            try {
                config_.output()->flush();
            } catch (const std::exception& e) {
                mark_degraded(e.what());
            } catch (...) {
                mark_degraded("unknown exception while flushing");
            }
        }

        [[nodiscard]] bool should_log(LogLevel lvl, const std::string_view prefix) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.threshold()) &&
                   config_.prefix_filter().accepts(prefix);
        }

        /// Re-checks the stream: a console sink has nothing to reopen, so recovery is
        /// simply the stream becoming usable again.
        void maintain() noexcept override {
            std::lock_guard lock{mutex_};
            if (config_.output()->good()) {
                note_success();
                set_status(SinkStatus::Healthy, {});
            }
        }

        /// Replaces the sink's config at runtime.
        void set_config(ConsoleSinkConfig cfg) noexcept {
            config_ = std::move(cfg);
            publish_threshold(config_.threshold());
        }

        /// The sink's current config.
        [[nodiscard]] constexpr const ConsoleSinkConfig& config() const noexcept {
            return config_;
        }

    private:
        ConsoleSinkConfig config_;
        mutable std::mutex mutex_;
        std::string format_buffer_;

        /// Records a write/flush failure and marks the sink Degraded with reason. Caller
        /// must hold mutex_.
        ///
        /// note_failure() is only called on the Healthy -> Degraded edge, not on every
        /// failing call: it arms the janitor's retry backoff, and a console sink keeps
        /// receiving events while Degraded (the Logger only skips Dead sinks). Bumping it
        /// on every failing write would re-arm retry_at_ms from "now" on each one, so once
        /// traffic pinned it at the 60s cap, retry_due() would never see the deadline pass
        /// while events kept arriving -- the sink would stay Degraded forever even after
        /// the stream recovered. Gating on the edge means the backoff instead reflects
        /// failed recovery attempts, matching what note_failure() is documented to record.
        void mark_degraded(const std::string_view reason) noexcept {
            if (get_status() == SinkStatus::Healthy) {
                note_failure();
            }
            set_status(SinkStatus::Degraded, reason);
        }

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
