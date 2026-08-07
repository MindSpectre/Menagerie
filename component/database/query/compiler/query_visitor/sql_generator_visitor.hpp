#pragma once

#include <memory>
#include <tuple>

#include <dialect_concepts.hpp>
#include <param_mode.hpp>
#include <sql_params.hpp>

#include "query_visitor.hpp"

namespace menagerie::db {

    /**
     * @brief Concrete QueryVisitor that renders an expression tree as SQL text for DialectT.
     *
     * Implements the visit_xxx_impl(...) hooks QueryVisitor's visit() overloads call: each hook appends the
     * SQL fragment for its piece of syntax (a keyword, an operator, a delimiter, ...) to sql_, quoting
     * identifiers and formatting/binding values through DialectT. StringT is beavers::InlineString<MaxLen>
     * on the constexpr path (QueryCompiler::compile_static) and std::pmr::string on the runtime path
     * (QueryCompiler::compile_dynamic); Mode picks how literal values are handled: Inline formats them
     * directly into sql_, Tuple leaves a placeholder and returns them via capture_param()/cat_params() for
     * the caller to collect into a std::tuple, and Sink leaves a placeholder and pushes them into sink_.
     */
    template <IsSqlDialect DialectT, Appendable StringT = std::string, ParamMode Mode = ParamMode::Inline>
    class SqlGeneratorVisitor final : public QueryVisitor<SqlGeneratorVisitor<DialectT, StringT, Mode>> {
    public:
        // Constexpr path - no sink, no PMR
        constexpr SqlGeneratorVisitor() = default;

        // Runtime path - PMR string, no sink (Inline mode)
        /// Runtime path with no parameter sink (Inline/Tuple mode): sql_ is allocated from mr.
        explicit SqlGeneratorVisitor(std::pmr::memory_resource* mr)
            : sql_{mr} {
        }

        // Runtime path - with sink and PMR (Sink mode)
        /// Runtime path with a parameter sink (Sink mode): sql_ is allocated from mr, captured values go to
        /// sink.
        SqlGeneratorVisitor(ParamSink* sink, std::pmr::memory_resource* mr)
            : sql_{mr},
              sink_{sink} {
        }

        /// Splits *this into its {sql, param_count} results, moving/copying per Self's value category.
        template <typename Self>
        constexpr auto decompose(this Self&& self) {
            return std::forward_as_tuple(std::forward_like<Self>(self.sql_),
                                         std::forward_like<Self>(self.param_count_));
        }

        // Get results
        /// The generated SQL text so far.
        template <typename Self>
        [[nodiscard]] constexpr auto sql(this Self&& self) {
            return std::forward_like<Self>(self.sql_);
        }

        /// Number of placeholders emitted so far (Tuple/Sink modes only; always 0 in Inline mode).
        [[nodiscard]] constexpr std::size_t param_count() const noexcept {
            return param_count_;
        }

        // -------- Compile-time parameter collection helpers --------
        /// In Tuple mode, wraps val as a 1-element tuple to be concatenated into the result via cat_params();
        /// a no-op empty tuple in every other mode.
        template <typename ValTp>
        [[nodiscard]] constexpr auto capture_param([[maybe_unused]] ValTp&& val) const noexcept {
            if constexpr (Mode == ParamMode::Tuple) {
                return std::make_tuple(std::forward<ValTp>(val));
            } else {
                return std::tuple<>{};
            }
        }

        /// The empty-parameter-list result every leaf/DML visit() returns when it captures nothing itself.
        static constexpr auto no_params() noexcept {
            return std::tuple<>{};
        }

        /// Concatenates the parameter tuples collected from an expression's sub-nodes, in visitation order
        /// (Tuple mode only; an empty tuple in every other mode).
        template <typename... Tuples>
        constexpr auto cat_params(Tuples&&... tuples) const noexcept {
            if constexpr (Mode == ParamMode::Tuple) {
                return std::tuple_cat(std::forward<Tuples>(tuples)...);
            } else {
                return std::tuple<>{};
            }
        }

        // ALL visit_*_impl methods are PUBLIC - the CRTP base calls them via derived()
        /// Emits `table.name` (both quoted via DialectT), then the alias per visit_alias_impl.
        constexpr void visit_column_impl(std::string_view name, std::string_view table, std::string_view alias);
        /// Emits value per Mode: formatted inline, a numbered Tuple placeholder, or pushed to sink_ with a
        /// Sink placeholder.
        constexpr void visit_value_impl(const FieldValue& value);
        /// @overload
        constexpr void visit_value_impl(FieldValue&& value);
        /// Emits `NULL`.
        constexpr void visit_null_impl();
        /// Emits a Tuple-mode placeholder for an explicit ParamPlaceholder<T> with no value attached.
        constexpr void visit_param_placeholder_impl();

        /// Emits `table.*` or `*`.
        constexpr void visit_all_columns_impl(std::string_view table);

        /// Emits table->table_name() (quoted); does nothing for a null table pointer.
        constexpr void visit_table_impl(const DynamicTablePtr& table);
        /// Emits table_name (quoted); does nothing for an empty name.
        constexpr void visit_table_impl(std::string_view table_name);

        /// Emits table.table_name() (quoted).
        template <IsStaticTable TableTp>
        constexpr void visit_table_impl(const TableTp& table);

        /// Emits ` AS alias` (quoted); does nothing for an empty alias.
        constexpr void visit_alias_impl(std::string_view alias);

        // Expression helpers
        /// Emits `(`, opening a parenthesized binary expression.
        constexpr void visit_binary_expr_start();
        /// Emits `)`, closing a parenthesized binary expression.
        constexpr void visit_binary_expr_end();
        /// Emits nothing; opening hook bracketing a unary expression (no parens, unlike
        /// visit_binary_expr_start/end).
        constexpr void visit_unary_expr_start() {
            beavers::force_non_static(this);
        }
        /// Emits nothing; closing hook bracketing a unary expression (no parens, unlike
        /// visit_binary_expr_start/end).
        constexpr void visit_unary_expr_end() {
            beavers::force_non_static(this);
        }
        /// Emits `(`, opening a parenthesized subquery.
        constexpr void visit_subquery_start();
        /// Emits `)`, closing a parenthesized subquery.
        constexpr void visit_subquery_end();
        /// Emits `EXISTS (`.
        constexpr void visit_exists_start();
        /// Emits `)`, closing an EXISTS subquery.
        constexpr void visit_exists_end();

        // Binary operators - each emits its SQL token surrounded by single spaces
        /// Emits ` = `.
        constexpr void visit_binary_op_impl(OpEqual);
        /// Emits ` != `.
        constexpr void visit_binary_op_impl(OpNotEqual);
        /// Emits ` < `.
        constexpr void visit_binary_op_impl(OpLess);
        /// Emits ` <= `.
        constexpr void visit_binary_op_impl(OpLessEqual);
        /// Emits ` > `.
        constexpr void visit_binary_op_impl(OpGreater);
        /// Emits ` >= `.
        constexpr void visit_binary_op_impl(OpGreaterEqual);
        /// Emits ` AND `.
        constexpr void visit_binary_op_impl(OpAnd);
        /// Emits ` OR `.
        constexpr void visit_binary_op_impl(OpOr);
        /// Emits ` LIKE `.
        constexpr void visit_binary_op_impl(OpLike);
        /// Emits ` NOT LIKE `.
        constexpr void visit_binary_op_impl(OpNotLike);
        /// Emits ` IN `.
        constexpr void visit_binary_op_impl(OpIn);

        // Unary operators
        /// Emits `NOT `.
        constexpr void visit_unary_op_impl(OpNot);
        /// Emits ` IS NULL`.
        constexpr void visit_unary_op_impl(OpIsNull);
        /// Emits ` IS NOT NULL`.
        constexpr void visit_unary_op_impl(OpIsNotNull);

        // Special operators
        /// Emits ` BETWEEN `.
        constexpr void visit_between_impl();
        /// Emits ` AND ` (used both for BETWEEN...AND and boolean AND via visit_binary_op_impl(OpAnd)).
        constexpr void visit_and_impl();
        /// Emits ` IN (`.
        constexpr void visit_in_list_start();
        /// Emits `)`, closing an IN list.
        constexpr void visit_in_list_end();
        /// Emits `, `, separating IN-list values.
        constexpr void visit_in_list_separator();

        // Aggregate functions
        /// Emits `COUNT(` and, if distinct, `DISTINCT `.
        constexpr void visit_count_impl(bool distinct);
        /// Emits `SUM(`.
        constexpr void visit_sum_impl();
        /// Emits `AVG(`.
        constexpr void visit_avg_impl();
        /// Emits `MAX(`.
        constexpr void visit_max_impl();
        /// Emits `MIN(`.
        constexpr void visit_min_impl();
        /// Emits `)` closing the aggregate call, then the alias per visit_alias_impl.
        constexpr void visit_aggregate_end(std::string_view alias);

        // Query parts
        /// Emits `SELECT ` and, if distinct, `DISTINCT `.
        constexpr void visit_select_start(bool distinct);
        /// Emits nothing; closing hook for the SELECT column list.
        constexpr void visit_select_end();
        /// Emits ` FROM `.
        constexpr void visit_from_start();
        /// Emits nothing; closing hook for FROM.
        constexpr void visit_from_end();
        /// Emits ` WHERE `.
        constexpr void visit_where_start();
        /// Emits nothing; closing hook for WHERE.
        constexpr void visit_where_end();
        /// Emits ` GROUP BY `.
        constexpr void visit_group_by_start();
        /// Emits nothing; closing hook for GROUP BY.
        constexpr void visit_group_by_end();
        /// Emits ` HAVING `.
        constexpr void visit_having_start();
        /// Emits nothing; closing hook for HAVING.
        constexpr void visit_having_end();
        /// Emits ` ORDER BY `.
        constexpr void visit_order_by_start();
        /// Emits nothing; closing hook for ORDER BY.
        constexpr void visit_order_by_end();
        /// Emits ` ASC` or ` DESC`.
        constexpr void visit_order_direction_impl(OrderDirection dir);
        /// Emits the dialect's LIMIT/OFFSET clause via DialectT::limit_clause.
        constexpr void visit_limit_impl(std::size_t limit, std::size_t offset);

        // Joins
        /// Emits ` INNER JOIN `/` LEFT JOIN `/` RIGHT JOIN `/` FULL OUTER JOIN `/` CROSS JOIN ` per type.
        constexpr void visit_join_start(JoinType type);
        /// Emits ` ON `.
        constexpr void visit_join_on();
        /// Emits nothing; closing hook for a JOIN clause.
        constexpr void visit_join_end();

        // DML
        /// Emits `INSERT INTO `.
        constexpr void visit_insert_start();
        /// Emits ` (col1, col2, ...) VALUES ` with each name quoted via DialectT.
        constexpr void visit_insert_columns(const std::vector<std::string>& columns);
        /// @overload
        constexpr void visit_insert_columns(std::vector<std::string>&& columns);
        /// Emits `(v1, v2, ...), (v1, v2, ...), ...`, one parenthesized row per entry, each value through
        /// visit_value_impl.
        constexpr void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows);
        /// @overload
        constexpr void visit_insert_values(std::vector<std::vector<FieldValue>>&& rows);
        /// Emits nothing; closing hook for INSERT.
        constexpr void visit_insert_end();

        /// Emits `UPDATE `.
        constexpr void visit_update_start();
        /// Emits ` SET col1 = v1, col2 = v2, ...` with each column quoted and each value through
        /// visit_value_impl.
        constexpr void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments);
        /// @overload
        constexpr void visit_update_set(std::vector<std::pair<std::string, FieldValue>>&& assignments);
        /// Emits nothing; closing hook for UPDATE.
        constexpr void visit_update_end();

        /// Emits `DELETE FROM `.
        constexpr void visit_delete_start();
        /// Emits nothing; closing hook for DELETE.
        constexpr void visit_delete_end();

        /// Emits `CASE`.
        constexpr void visit_case_start();
        /// Emits ` END`.
        constexpr void visit_case_end();
        /// Emits ` WHEN `.
        constexpr void visit_when_start();
        /// Emits ` THEN `.
        constexpr void visit_when_then();
        /// Emits nothing; closing hook for a WHEN clause.
        constexpr void visit_when_end();
        /// Emits ` ELSE `.
        constexpr void visit_else_start();
        /// Emits nothing; closing hook for ELSE.
        constexpr void visit_else_end();

        // CTE (Common Table Expression)
        /// Emits `WITH ` and, if recursive, `RECURSIVE `.
        constexpr void visit_cte_start(bool recursive);
        /// Emits the CTE's name, quoted via DialectT.
        constexpr void visit_cte_name_impl(std::string_view name);
        /// Emits ` AS (`.
        constexpr void visit_cte_as_start();
        /// Emits `)`, closing a CTE's body.
        constexpr void visit_cte_as_end();
        /// Emits ` `, separating a CTE clause from the query that follows it.
        constexpr void visit_cte_end();

        // DDL - CREATE TABLE
        /// Emits `CREATE TABLE ` and, if requested, `IF NOT EXISTS `.
        constexpr void visit_create_table_start(bool if_not_exists);
        /// Emits `(col type [PRIMARY KEY] [NOT NULL] [UNIQUE] [DEFAULT ...] [REFERENCES ...], ...)` for a
        /// DynamicTable's runtime-registered fields.
        constexpr void visit_create_table_columns(const DynamicTablePtr& table);

        /// Emits the same column-definition list as the DynamicTable overload, but reading a StaticTable's
        /// compile-time field schema pack via for_each_field.
        template <IsStaticTable TableTp>
        constexpr void visit_create_table_columns(const TableTp& table);

        /// Emits nothing for this dialect; closing hook for CREATE TABLE (other dialects might add
        /// ENGINE/CHARSET/etc. here).
        constexpr void visit_create_table_end();

        // DDL - DROP TABLE
        /// Emits `DROP TABLE ` and, if requested, `IF EXISTS `.
        constexpr void visit_drop_table_start(bool if_exists);
        /// Emits ` CASCADE` if requested.
        constexpr void visit_drop_table_end(bool cascade);

        // Set operations
        /// Emits ` UNION `/` UNION ALL `/` INTERSECT `/` EXCEPT ` per op.
        constexpr void visit_set_op_impl(SetOperation op);

        // Column separator
        /// Emits `, `, separating items in a column/expression list.
        constexpr void visit_column_separator();

    private:
        StringT sql_{};
        ParamSink* sink_         = nullptr;
        std::size_t param_count_ = 0;
    };
}  // namespace menagerie::db

#include "detail/sql_generator_visitor.inl"
