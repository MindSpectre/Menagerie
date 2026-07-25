#pragma once
#include <memory>
#include <menagerie/beavers>
#include <optional>

#include <libpq-fe.h>

#include "views/postgres_result_views.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief Owning wrapper around a libpq PGresult, exposing row/column access
     *        through RowView/FieldView without copying the result data.
     *
     * Move-only (via beavers::NonCopyable); the underlying PGresult is released
     * through PQclear() on destruction.
     */
    class ResultBlock : beavers::NonCopyable {
    public:
        /// Takes ownership of an existing PGresult (e.g. from PQexec()/PQgetResult()).
        explicit ResultBlock(PGresult* r)
            : res_{r} {
        }

        /// Whether the result has zero rows.
        [[nodiscard]] bool empty() const noexcept {
            return rows() == 0;
        }

        /// Number of rows in the result.
        [[nodiscard]] std::size_t rows() const noexcept {
            return static_cast<std::size_t>(PQntuples(res_.get()));
        }

        /// Number of columns in the result.
        [[nodiscard]] std::size_t cols() const noexcept {
            return static_cast<std::size_t>(PQnfields(res_.get()));
        }

        /// View onto row i. The view is only valid while this ResultBlock lives.
        [[nodiscard]] RowView get_row(const std::size_t i) const {
            return RowView{res_.get(), i};
        }

        /**
         * @brief Decodes column c of row r as T, or std::nullopt if the value is SQL NULL.
         * @throw std::runtime_error if the value is present but fails to decode as T
         *        (e.g. malformed numeric/float text).
         */
        template <class T>
        [[nodiscard]] std::optional<T> get_opt(const std::size_t r, const std::size_t c) const {
            const auto f = get_row(r)[c];
            if (f.is_null())
                return std::nullopt;
            return f.as<T>();
        }

        /**
         * @brief Decodes column `column` of row `row` as T.
         * @throw std::runtime_error if the value is SQL NULL, or if it is present
         *        but fails to decode as T (e.g. malformed numeric/float text).
         */
        template <class T>
        [[nodiscard]] T get(const std::size_t row, const std::size_t column) const {
            const auto f = get_row(row)[column];
            if (f.is_null())
                throw std::runtime_error("get() failed");
            return f.as<T>();
        }

        /// The underlying PGresult; ownership stays with this ResultBlock.
        [[nodiscard]] PGresult* raw() const noexcept {
            return res_.get();
        }

    private:
        struct PgResultDeleter {
            void operator()(PGresult* r) const noexcept {
                if (r)
                    PQclear(r);
            }
        };
        std::unique_ptr<PGresult, PgResultDeleter> res_;
    };
}  // namespace menagerie::db::postgres
