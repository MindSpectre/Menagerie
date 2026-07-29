#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>

#include "config/file_sink_config.hpp"
#include "detail/file_naming.hpp"

namespace menagerie::crow {

    /**
     * @brief File sink with automatic rotation
     *
     * @tparam EntryType Defines which metadata to include (e.g., DetailedEntry, LightEntry)
     *
     * Features:
     * - Automatic file rotation when max_file_size is reached
     * - Timestamp-based file naming (app_2025-01-18T10:30:45.log)
     * - Thread-safe writing
     * - Automatic directory creation
     *
     * Rotation modes, from FileSinkConfig's rotate_file() and add_time_to_filename():
     * - false, false: always app.log; never rotates.
     * - false, true:  app_TIME.log, timestamp fixed at open; never rotates.
     * - true, false:  app.log -> app_1.log -> app_2.log ...; the active file is the
     *                 highest index, and startup resumes the highest index found on
     *                 disk while it still has room under max_file_size.
     * - true, true:   app_TIME.log; a rotation landing in the same second as the
     *                 current file keeps writing it instead of fanning out.
     */
    template <detail::EntryConcept EntryType>
    class FileSink final : public Sink {
    public:
        /// Opens the configured file, creating parent directories as needed. A sink that
        /// cannot open its file starts Dead and is retried by the Logger's janitor; it
        /// never throws and never terminates the process.
        template <typename FileSinkConfigTp = FileSinkConfig>
            requires std::constructible_from<FileSinkConfig, FileSinkConfigTp>
        explicit FileSink(FileSinkConfigTp&& cfg) noexcept
            : config_{std::forward<FileSinkConfigTp>(cfg)} {
            publish_threshold(config_.threshold());
            open_from_config();
        }

        ~FileSink() override {
            std::lock_guard lock{mutex_};
            if (file_stream_.is_open()) {
                file_stream_.flush();
                file_stream_.close();
            }
        }

        void maintain() noexcept override {
            if (!retry_due(dispatch_hint(), detail::steady_now_ms())) {
                return;
            }
            force_maintain();
        }

        /// Runs maintenance now, ignoring the backoff deadline.
        void force_maintain() noexcept {
            if (get_status() == SinkStatus::Dead) {
                open_from_config();
                return;
            }
            rotate_now();
        }

        void process(const LogEvent& event) noexcept override {
            if (get_status() == SinkStatus::Dead) {
                add_undelivered(1);
                return;
            }
            if (!write_guarded(event)) {
                add_undelivered(1);  // this event died with the sink; count it too
                return;
            }
            rotate_if_needed();
        }

        void process_batch(const std::shared_ptr<std::vector<LogEvent>>& batch) noexcept override {
            if (get_status() == SinkStatus::Dead) {
                add_undelivered(batch->size());
                return;
            }
            for (std::size_t i = 0; i < batch->size(); ++i) {
                if (!write_guarded((*batch)[i])) {
                    add_undelivered(batch->size() - i);  // this event and every one after it
                    return;                              // the sink is Dead now; the rest of the batch is lost
                }
            }
            rotate_if_needed();
        }

        void flush() noexcept override {
            std::lock_guard lock{mutex_};
            if (file_stream_.is_open()) {
                file_stream_.flush();
            }
        }

        /// True if lvl/prefix pass the configured threshold and prefix filter.
        [[nodiscard]] bool should_log(LogLevel lvl, const std::string_view prefix) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.threshold()) &&
                   config_.prefix_filter().accepts(prefix);
        }

        /// Replaces the sink's config at runtime and republishes its threshold
        /// immediately. Does not reopen the file or re-derive file_path(); a changed
        /// file/rotation policy only takes effect on the next rotation.
        void set_config(FileSinkConfig cfg) noexcept {
            config_ = std::move(cfg);
            publish_threshold(config_.threshold());
        }

        /// The sink's current config.
        [[nodiscard]] constexpr const FileSinkConfig& config() const noexcept {
            return config_;
        }

        /// The currently open file's path (post timestamp-naming, if enabled). Empty
        /// while the sink is Dead: there is no file backing it yet.
        [[nodiscard]] const std::filesystem::path& file_path() const noexcept {
            return file_path_;
        }

    private:
        /// Resolves the path to open from config and opens it, recording failure as Dead.
        void open_from_config() noexcept {
            std::error_code ec;
            const std::filesystem::path path = detail::initial_path(config_, ec);
            if (ec) {
                fail(SinkStatus::Dead, "resolve log path: " + ec.message());
                return;
            }
            open_stream(path, SinkStatus::Dead);
        }

        /**
         * @brief Opens path and adopts it as the active stream.
         *
         * The new stream is opened before the old one is closed, so a failed open leaves
         * the sink writing where it already was. on_failure selects Dead (there is no
         * usable stream) or Degraded (rotation failed but the current file still works).
         */
        void open_stream(const std::filesystem::path& path, const SinkStatus on_failure) noexcept {
            std::error_code ec;
            if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    fail(on_failure, "create " + parent.string() + ": " + ec.message());
                    return;
                }
            }

            std::ofstream next;
            next.open(path, std::ios::out | std::ios::app);
            if (!next.is_open()) {
                fail(on_failure, "open " + path.string() + " failed");
                return;
            }

            std::lock_guard lock{mutex_};
            if (file_stream_.is_open()) {
                file_stream_.flush();
                file_stream_.close();
            }
            file_stream_ = std::move(next);
            // The 64 KB buffer cannot back two live streams, so it is attached only after
            // the previous stream is closed and before any output reaches the new one.
            file_stream_.rdbuf()->pubsetbuf(stream_buffer_, sizeof(stream_buffer_));
            file_path_ = path;

            std::error_code size_ec;
            const auto size = std::filesystem::file_size(path, size_ec);
            bytes_written_  = size_ec ? 0 : size;

            note_success();
            set_status(SinkStatus::Healthy, {});
        }

        /// Records a failed maintenance attempt and schedules the next retry.
        void fail(const SinkStatus status, const std::string_view reason) noexcept {
            note_failure();
            set_status(status, reason);
        }

        /// Gate in front of rotate_now(): a Degraded sink only retries rotation once
        /// its backoff deadline has passed, so a persistently blocked rotation target
        /// does not get re-attempted on every single write. A Healthy sink crossing
        /// the size threshold has no backoff to wait on, so it rotates immediately.
        /// force_maintain() calls rotate_now() directly to bypass this gate, as its
        /// own doc promises.
        void rotate_if_needed() noexcept {
            if (get_status() == SinkStatus::Degraded && !retry_due(dispatch_hint(), detail::steady_now_ms())) {
                return;
            }
            rotate_now();
        }

        /// Rotates the active file right now if it has grown past max_file_size(),
        /// without checking the backoff gate. A failed open leaves the sink writing
        /// to the current file and marks it Degraded, so the next maintenance pass
        /// retries after the backoff.
        void rotate_now() noexcept {
            if (!config_.rotate_file() || bytes_written_ < config_.max_file_size()) {
                return;
            }

            std::error_code ec;
            const std::filesystem::path next = detail::next_path(config_, file_path_, ec);
            if (ec) {
                fail(SinkStatus::Degraded, "resolve rotation target: " + ec.message());
                return;
            }
            if (next == file_path_) {
                return;  // timestamped mode within the same second: carry on writing
            }
            open_stream(next, SinkStatus::Degraded);
        }

        /// Formats and writes one event. Returns false if the sink died doing it.
        [[nodiscard]] bool write_guarded(const LogEvent& event) noexcept {
            try {
                if (!should_log(event.level, event.prefix.view())) {
                    return true;
                }
                auto entry = make_entry_from_event<EntryType>(event);
                entry.format_into(format_buffer_);

                std::lock_guard lock{mutex_};
                file_stream_ << format_buffer_;
                bytes_written_ += format_buffer_.size();
                if (config_.flush_each_entry()) {
                    file_stream_.flush();
                }
                return true;
            } catch (const std::exception& e) {
                fail(SinkStatus::Dead, e.what());
                return false;
            } catch (...) {
                fail(SinkStatus::Dead, "unknown exception while writing");
                return false;
            }
        }

        FileSinkConfig config_;
        std::ofstream file_stream_;
        std::filesystem::path file_path_;
        std::uint64_t bytes_written_ = 0;  // tracked in-process: no tellp(), no file_size() per write
        std::mutex mutex_;
        std::string format_buffer_;                    // Reused across process() calls (no TL dependency)
        alignas(64) char stream_buffer_[64 * 1024]{};  // 64KB static buffer, cache-line aligned
    };
}  // namespace menagerie::crow
