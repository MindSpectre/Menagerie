#pragma once
#include <algorithm>
#include <deque>
#include <functional>
#include <vector>

/// Bounded-memory streaming sort: SlidingWindowSorter keeps a running sorted
/// window over data arriving in batches.
namespace menagerie::algorithms {
    /// @brief Default comparator: uses T::comp(a, b) if the type provides a static
    /// comp, otherwise falls back to operator<.
    template <typename T>
    struct DefaultComparator {
        /// Orders `a` before `b` via `T::comp` if available, otherwise `operator<`.
        bool operator()(const T& a, const T& b) const {
            if constexpr (requires { T::comp(a, b); }) {
                return T::comp(a, b);
            } else {
                return a < b;
            }
        }
    };

    /// @brief Tunables for a SlidingWindowSorter instance.
    template <typename T>
    struct SlidingWindowConfig {
        std::size_t window_size                            = 1024;  ///< Entries retained before flushing.
        std::size_t batch_size                             = 512;   ///< Entries buffered before a sort/merge pass runs.
        bool enable_sorting                                = true;  ///< If false, batches are appended unsorted.
        std::function<bool(const T&, const T&)> comparator = DefaultComparator<T>{};  ///< Ordering used when sorting.

        // Performance tuning
        bool use_inplace_merge      = true;  ///< Reuse reserved capacity for the merge instead of a scratch buffer.
        std::size_t merge_threshold = 64;    ///< Below this many new entries, use insertion sort instead of std::sort.
    };


    /**
     * @brief Bounded-memory streaming sort over data arriving in batches.
     *
     * Keeps a running sorted window and flushes the oldest entries to a consumer
     * callback once the window fills up.
     */
    template <typename T>
    class SlidingWindowSorter {
    public:
        using value_type        = T;  ///< The sorted element type.
        using comparator_type   = std::function<bool(const T&, const T&)>;  ///< Ordering predicate signature.
        using consumer_function = std::function<void(const std::vector<T>&)>;  ///< Flush callback signature.

    private:
        SlidingWindowConfig<T> config_;
        std::vector<T> sorted_window_;
        std::vector<T> new_entries_;
        std::vector<T> merge_buffer_;
        consumer_function consumer_;

        // Statistics
        std::size_t total_processed_  = 0;
        std::size_t merge_operations_ = 0;
        std::size_t sort_operations_  = 0;

    public:
        /// @brief Constructs a sorter with the given config; entries flushed once the
        /// window fills are handed to consumer.
        explicit SlidingWindowSorter(SlidingWindowConfig<T> config, consumer_function consumer)
            : config_(std::move(config)),
              consumer_(std::move(consumer)) {
            sorted_window_.reserve(config_.window_size);
            new_entries_.reserve(config_.batch_size);
            merge_buffer_.reserve(config_.window_size + config_.batch_size);
        }

        /// Adds a batch of entries; triggers a sort/merge pass once batch_size is reached.
        void add_entries(std::vector<T> entries) {
            if (entries.empty())
                return;

            // Move entries to new_entries buffer
            new_entries_.insert(
                new_entries_.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));

            // Process if we have enough entries
            if (should_process()) {
                process_batch();
            }
        }

        /// Adds a single entry; triggers a sort/merge pass once batch_size is reached.
        void add_entry(T entry) {
            new_entries_.emplace_back(std::move(entry));

            if (should_process()) {
                process_batch();
            }
        }

        /// Forces processing and output of all remaining buffered and windowed entries.
        void flush() {
            if (!new_entries_.empty() || !sorted_window_.empty()) {
                process_batch(true /* force_all */);
            }
        }

        /// @brief Running counters for a sorter instance.
        struct Statistics {
            std::size_t total_processed;   ///< Entries handed to the consumer so far.
            std::size_t merge_operations;  ///< Number of sort/merge passes that merged with an existing window.
            std::size_t sort_operations;   ///< Number of sort/merge passes performed.
            double avg_merge_efficiency;   ///< merge_operations / sort_operations, or 0 if none yet.
        };

        /// Returns a snapshot of the sorter's running counters.
        [[nodiscard]] Statistics get_statistics() const {
            return {total_processed_,
                    merge_operations_,
                    sort_operations_,
                    sort_operations_ > 0
                        ? static_cast<double>(merge_operations_) / static_cast<double>(sort_operations_)
                        : 0.0};
        }

        /// Flushes any pending entries under the old config, then swaps in new_config.
        void reconfigure(SlidingWindowConfig<T> new_config) {
            flush();  // Process any pending entries with old config
            config_ = std::move(new_config);

            // Resize buffers if needed
            sorted_window_.reserve(config_.window_size);
            new_entries_.reserve(config_.batch_size);
            merge_buffer_.reserve(config_.window_size + config_.batch_size);
        }

    private:
        [[nodiscard]] bool should_process() const {
            return new_entries_.size() >= config_.batch_size;
        }

        void process_batch(const bool force_all = false) {
            if (new_entries_.empty() && sorted_window_.empty())
                return;

            if (config_.enable_sorting) {
                sort_and_merge();
            } else {
                // No sorting - just append new entries to window
                sorted_window_.insert(sorted_window_.end(),
                                      std::make_move_iterator(new_entries_.begin()),
                                      std::make_move_iterator(new_entries_.end()));
                new_entries_.clear();
            }

            // Determine how many entries to output
            std::size_t output_count;
            if (force_all) {
                output_count = sorted_window_.size();
            } else {
                output_count = calculate_output_count();
            }

            if (output_count > 0) {
                output_entries(output_count);
            }

            // Maintain window size
            maintain_window_size();
        }

        void sort_and_merge() {
            if (new_entries_.empty())
                return;

            // Sort new entries
            if (new_entries_.size() <= config_.merge_threshold) {
                insertion_sort(new_entries_.begin(), new_entries_.end(), config_.comparator);
            } else {
                std::sort(new_entries_.begin(), new_entries_.end(), config_.comparator);
            }
            sort_operations_++;

            if (sorted_window_.empty()) {
                // First batch - just move
                sorted_window_ = std::move(new_entries_);
                new_entries_.clear();
            } else {
                // Merge with existing sorted window
                merge_with_window();
                merge_operations_++;
            }
        }

        void merge_with_window() {
            if (config_.use_inplace_merge && can_use_inplace_merge()) {
                // In-place merge (memory efficient)
                perform_inplace_merge();
            } else {
                // Standard merge (faster but uses more memory)
                perform_standard_merge();
            }
            new_entries_.clear();
        }

        [[nodiscard]] bool can_use_inplace_merge() const {
            // Check if in-place merge would be beneficial
            return sorted_window_.capacity() >= sorted_window_.size() + new_entries_.size();
        }

        void perform_inplace_merge() {
            // Append new entries to sorted window
            const std::size_t old_size = sorted_window_.size();
            sorted_window_.insert(sorted_window_.end(),
                                  std::make_move_iterator(new_entries_.begin()),
                                  std::make_move_iterator(new_entries_.end()));

            // In-place merge
            std::inplace_merge(sorted_window_.begin(),
                               sorted_window_.begin() + static_cast<long>(old_size),
                               sorted_window_.end(),
                               config_.comparator);
        }

        void perform_standard_merge() {
            merge_buffer_.clear();
            merge_buffer_.reserve(sorted_window_.size() + new_entries_.size());

            std::merge(sorted_window_.begin(),
                       sorted_window_.end(),
                       new_entries_.begin(),
                       new_entries_.end(),
                       std::back_inserter(merge_buffer_),
                       config_.comparator);

            sorted_window_ = std::move(merge_buffer_);
            merge_buffer_.clear();
        }

        [[nodiscard]] size_t calculate_output_count() const {
            if (sorted_window_.size() < config_.window_size) {
                return 0;  // Window not exceeded, keep everything
            }

            // Calculate how many entries to flush to maintain window size
            size_t excess = sorted_window_.size() - config_.window_size;

            // Flush at least batch_size entries when window is exceeded
            return std::max(excess, config_.batch_size);
        }

        void output_entries(std::size_t count) {
            if (count == 0 || sorted_window_.empty())
                return;

            // Create output vector
            std::vector<T> output;
            output.reserve(count);

            auto end_it = sorted_window_.begin() + static_cast<long>(std::min(count, sorted_window_.size()));
            output.insert(
                output.end(), std::make_move_iterator(sorted_window_.begin()), std::make_move_iterator(end_it));

            // Remove output entries from window
            sorted_window_.erase(sorted_window_.begin(), end_it);

            // Call consumer
            const std::size_t output_size = output.size();
            consumer_(std::move(output));
            total_processed_ += output_size;
        }

        void maintain_window_size() {
            if (sorted_window_.size() > config_.window_size) {
                const std::size_t excess = sorted_window_.size() - config_.window_size;
                sorted_window_.erase(sorted_window_.begin(), sorted_window_.begin() + static_cast<long>(excess));
            }
        }

        // Optimized insertion sort for small sequences
        template <typename Iterator, typename Compare>
        void insertion_sort(Iterator first, Iterator last, const Compare& comp) {
            for (auto it = first + 1; it != last; ++it) {
                auto key = std::move(*it);
                auto pos = std::upper_bound(first, it, key, comp);
                std::move_backward(pos, it, it + 1);
                *pos = std::move(key);
            }
        }
    };
}  // namespace menagerie::algorithms
