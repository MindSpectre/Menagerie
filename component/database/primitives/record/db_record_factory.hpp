#pragma once

#include "db_record.hpp"

namespace menagerie::db {
    /**
     * @brief Factory for creating Record instances that share a common schema.
     *
     * Provides efficient Record creation by sharing a single Table schema instance
     * across multiple Record objects, avoiding schema duplication.
     */
    class [[deprecated]] RecordFactory {
    public:
        /// Constructs a RecordFactory sharing `schema` across every Record it creates.
        explicit RecordFactory(std::shared_ptr<const DynamicTable> schema)
            : schema_(std::move(schema)) {
        }

        /// Creates a single Record sharing the factory's schema.
        [[nodiscard]] Record create_record() const {
            return Record{schema_};
        }

        /// Creates `count` Records, all sharing the factory's schema, with
        /// the returned vector's capacity pre-reserved.
        std::vector<Record> create_batch(const size_t count) {
            std::vector<Record> records;
            records.reserve(count);

            for (size_t i = 0; i < count; ++i) {
                records.emplace_back(schema_);
            }

            return records;
        }

        /// The schema shared by every Record this factory creates; the
        /// constructor does not null-check `schema`, so this dereferences
        /// unchecked and requires the factory to have been constructed with
        /// a valid, non-null schema.
        [[nodiscard]] const DynamicTable& schema() const {
            return *schema_;
        }

    private:
        std::shared_ptr<const DynamicTable> schema_;
    };
}  // namespace menagerie::db
