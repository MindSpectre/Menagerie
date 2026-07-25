#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "detail/log_event.hpp"

namespace menagerie::crow {
    /**
     * @brief Base interface for all log sinks.
     *
     * Non-templated to allow heterogeneous storage in Logger.
     * Each sink implementation chooses its EntryType for formatting.
     *
     * Design:
     * - Logger stores vector<shared_ptr<Sink>>
     * - ConsoleSink<DetailedEntry>, FileSink<LightEntry> both inherit from Sink
     * - Consumer dispatches batches to sink strands via process_batch()
     * - Sink converts LogEvent -> EntryType -> formatted output
     */
    class Sink {
    public:
        virtual ~Sink() = default;

        /// Processes a single log event.
        virtual void process(const LogEvent& event) = 0;

        /**
         * @brief Processes a batch of log events.
         *
         * Default: iterates and calls process() per event.
         * Override for batch-optimized I/O (e.g., batch network sends).
         */
        virtual void process_batch(const std::shared_ptr<std::vector<LogEvent>>& batch) {
            for (const auto& event : *batch) {
                process(event);
            }
        }

        /**
         * @brief Flushes any buffered data.
         *
         * Called during:
         * - Logger shutdown
         * - Explicit flush requests
         * - Critical errors (ERR/FAT logs)
         */
        virtual void flush() = 0;

        /**
         * @brief Checks if this sink should process this log level, for early
         *        filtering before calling process().
         * @return true if the sink will process this level/prefix combination.
         */
        [[nodiscard]] virtual bool should_log(LogLevel lvl, std::string_view prefix) const noexcept = 0;
    };
}  // namespace menagerie::crow
