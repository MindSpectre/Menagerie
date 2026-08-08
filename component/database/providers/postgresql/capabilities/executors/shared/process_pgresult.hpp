#pragma once

#include <menagerie/beaver>

#include <postgres_errors.hpp>
#include <postgres_result.hpp>

namespace menagerie::db::postgres {

    /**
     * @brief Process PostgreSQL result and check for errors
     * @param result Raw PGresult pointer (takes ownership)
     * @return ResultBlock on success, ErrorContext on error
     *
     * This is a common helper used by both sync and async executors
     * to process PGresult objects and handle errors consistently.
     */
    inline beaver::Outcome<ResultBlock, ErrorContext> process_result(PGresult* result) {
        // Extract error if present
        if (auto error_ctx = extract_error(result)) {
            PQclear(result);
            return beaver::err(std::move(*error_ctx));
        }

        // Success - wrap result in ResultBlock (takes ownership)
        return ResultBlock{result};
    }
}  // namespace menagerie::db::postgres
