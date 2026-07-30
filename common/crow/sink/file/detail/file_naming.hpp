#pragma once

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "config/file_sink_config.hpp"

namespace menagerie::crow::detail {

    /// Index encoded in candidate's filename relative to stem_base: "app.log" -> 0,
    /// "app_3.log" -> 3, anything else (a foreign stem, a non-decimal suffix, or a
    /// timestamped name) -> nullopt.
    [[nodiscard]] inline std::optional<std::uint32_t> index_of(const std::filesystem::path& stem_base,
                                                               const std::filesystem::path& candidate) {
        const std::string base = stem_base.string();
        const std::string stem = candidate.stem().string();

        if (stem == base) {
            return 0;
        }
        if (!stem.starts_with(base + "_")) {
            return std::nullopt;
        }

        const std::string_view digits{stem.data() + base.size() + 1, stem.size() - base.size() - 1};
        if (digits.empty()) {
            return std::nullopt;
        }

        std::uint32_t value          = 0;
        const auto* const digits_end = digits.data() + digits.size();
        if (const auto [ptr, ec] = std::from_chars(digits.data(), digits_end, value);
            ec != std::errc{} || ptr != digits_end) {
            return std::nullopt;  // trailing/embedded non-digit, e.g. "app_x" or a timestamp
        }
        return value;
    }

    /// Path carrying index n relative to configured: 0 -> "app.log", 3 -> "app_3.log".
    [[nodiscard]] inline std::filesystem::path indexed_path(const std::filesystem::path& configured,
                                                            const std::uint32_t index) {
        if (index == 0) {
            return configured;
        }
        return configured.parent_path() /
               (configured.stem().string() + "_" + std::to_string(index) + configured.extension().string());
    }

    /// Path carrying a fresh timestamp: "app.log" -> "app_2026-07-29T10:00:00.log".
    [[nodiscard]] inline std::filesystem::path timestamped_path(const FileSinkConfig& config) {
        const std::filesystem::path& configured = config.file();
        const std::string time                  = chrono::LocalClock::current_time(config.time_format_in_file_name());
        return configured.parent_path() / (configured.stem().string() + "_" + time + configured.extension().string());
    }

    /**
     * @brief Path the sink should open when it starts.
     *
     * Indexed mode resumes the highest existing index while it still has room, so a
     * frequently restarted service does not leave one file per restart.
     *
     * @param config Config based on which path will be created.
     * @param ec Cleared on entry. Left clear if the target directory does not exist yet
     *           (the sink creates it on open, so that is not an error) or if the scan
     *           otherwise succeeds. Set if the directory exists but could not be scanned
     *           (e.g. permission denied) — the returned path is still a usable
     *           best-effort candidate, but the caller should treat a set ec as a signal
     *           that the resumed index may be stale.
     */
    [[nodiscard]] inline std::filesystem::path initial_path(const FileSinkConfig& config, std::error_code& ec) {
        ec.clear();

        if (config.add_time_to_filename()) {
            return timestamped_path(config);
        }
        if (!config.rotate_file()) {
            return config.file();
        }

        const std::filesystem::path parent   = config.file().parent_path();
        const std::filesystem::path scan_dir = parent.empty() ? std::filesystem::path{"."} : parent;
        const std::string base               = config.file().stem().string();

        std::uint32_t highest = 0;
        // exists()/directory_iterator leave ec clear for a missing directory (that is a
        // successful "not found" query, not a failure) and only set it for a genuine I/O
        // error, so no blanket ec.clear() is needed afterward. The end-iterator default
        // construction plus increment(ec) (rather than the range-for's throwing
        // operator++) means a mid-scan failure reports through ec instead of throwing.
        if (std::filesystem::exists(scan_dir, ec)) {
            std::filesystem::directory_iterator it{scan_dir, ec};
            const std::filesystem::directory_iterator end{};
            while (!ec && it != end) {
                // A directory can share a rotated file's name (e.g. a stray "app_5.log/"
                // left behind by something else); matching by extension alone would pick
                // it as the resume candidate, and opening a directory as a log file always
                // fails -- with initial_path()'s ec left clear, so the sink starts Dead and
                // every janitor retry re-resolves the same directory, never healing. The
                // error_code overload is required here, not the throwing one: initial_path()
                // is called from a noexcept context. A stat failure on this candidate (e.g.
                // a race removing it mid-scan) is treated the same as "not a match" rather
                // than aborting the whole scan over one entry.
                if (std::error_code type_ec;
                    it->path().extension() == config.file().extension() && it->is_regular_file(type_ec) && !type_ec) {
                    if (const auto index = index_of(base, it->path()); index && *index > highest) {
                        highest = *index;
                    }
                }
                it.increment(ec);
            }
        }

        std::filesystem::path candidate = indexed_path(config.file(), highest);
        std::error_code size_ec;
        if (const auto size = std::filesystem::file_size(candidate, size_ec);
            !size_ec && size >= config.max_file_size()) {
            return indexed_path(config.file(), highest + 1);
        }
        return candidate;
    }

    /**
     * @brief Path the sink should rotate into.
     * @return current when rotation must not create a new file — either rotation is off,
     *         or timestamped mode produced the same timestamp within one clock tick.
     */
    [[nodiscard]] inline std::filesystem::path
    next_path(const FileSinkConfig& config, const std::filesystem::path& current, std::error_code& ec) {
        ec.clear();

        if (!config.rotate_file()) {
            return current;
        }
        if (config.add_time_to_filename()) {
            return timestamped_path(config);  // equals current within the same second
        }

        const std::uint32_t index = index_of(config.file().stem(), current).value_or(0);
        return indexed_path(config.file(), index + 1);
    }
}  // namespace menagerie::crow::detail
