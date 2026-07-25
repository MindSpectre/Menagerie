#pragma once

#include <utility>

#include <db_core_objects.hpp>

#include "db_expressions_fwd.hpp"

namespace menagerie::db {
    /**
     * @brief CRTP base every expression node derives from.
     *
     * Ties a node to its Derived type so accept(visitor) (defined out of line, once the visitor type is
     * complete) and self() can be shared by every node without virtual dispatch.
     */
    template <class Derived>
    class Expression {
    public:
        using self_type = Derived;  ///< The concrete node type deriving from this CRTP base.

        /// Double-dispatch entry point: forwards *this to visitor.visit(*this).
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    protected:
        /// Casts *this to Derived&/&&, preserving const and value category.
        constexpr decltype(auto) self(this auto&& self) {
            using Self = decltype(self);
            using D    = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const Derived, Derived>;
            return static_cast<std::conditional_t<std::is_lvalue_reference_v<Self>, D&, D&&>>(self);
        }
    };

    // Binary operators
    /// Base tag every OpXxx operator type derives from; IsOperator<T> checks derivation from this.
    struct OpBase {};

    /// SQL `=`.
    struct OpEqual : OpBase {};

    /// SQL `<>`.
    struct OpNotEqual : OpBase {};

    /// SQL `<`.
    struct OpLess : OpBase {};

    /// SQL `<=`.
    struct OpLessEqual : OpBase {};

    /// SQL `>`.
    struct OpGreater : OpBase {};

    /// SQL `>=`.
    struct OpGreaterEqual : OpBase {};

    /// SQL `AND`.
    struct OpAnd : OpBase {};

    /// SQL `OR`.
    struct OpOr : OpBase {};

    /// SQL `LIKE`.
    struct OpLike : OpBase {};

    /// SQL `NOT LIKE`.
    struct OpNotLike : OpBase {};

    /// SQL `IN`.
    struct OpIn : OpBase {};

    /// SQL `NOT IN`.
    struct OpNotIn : OpBase {};

    // Unary operators - prefix (operator before operand)
    /// SQL `NOT`.
    struct OpNot : OpBase {};

    // Unary operators - postfix (operand before operator)
    /// SQL `IS NULL`. is_postfix tells UnaryExpr/QueryVisitor to emit the operand before the operator.
    struct OpIsNull : OpBase {
        static constexpr bool is_postfix = true;  ///< Always true: the operand precedes `IS NULL`.
    };

    /// SQL `IS NOT NULL`. is_postfix tells UnaryExpr/QueryVisitor to emit the operand before the operator.
    struct OpIsNotNull : OpBase {
        static constexpr bool is_postfix = true;  ///< Always true: the operand precedes `IS NOT NULL`.
    };

    /// JOIN kind for JoinExpr.
    enum class JoinType { INNER, LEFT, RIGHT, FULL, CROSS };

    /// Set-combination kind for SetOpExpr.
    enum class SetOperation { UNION, UNION_ALL, INTERSECT, EXCEPT };

    /**
     * @brief Wraps a raw value as an expression tree leaf.
     *
     * lit(value) builds one explicitly; detail::make_literal_if_needed builds one implicitly for operands
     * passed to select(...)/comparison operators that are not already expression nodes.
     */
    template <typename T>
    class Literal {
    public:
        /// Wraps v as the literal's value.
        template <typename ValueTp>
            requires std::constructible_from<T, ValueTp>
        constexpr explicit Literal(ValueTp&& v)
            : value{std::forward<ValueTp>(v)} {
        }

        /// Sets a display alias (e.g. `AS alias` in SELECT) and returns *this for chaining.
        template <typename Self, beavers::IsStringLike StringTp>
        constexpr auto&& as(this Self&& self, StringTp&& alias) {
            self.alias = std::forward<StringTp>(alias);
            return std::forward<Self>(self);
        }

        /// Double-dispatch entry point: forwards *this to visitor.visit(*this).
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

        T value;            ///< The wrapped value.
        std::string alias;  ///< Optional display alias; empty when unset.
    };

    // Deduction guide: Literal(x) deduces T from argument
    /// Deduces T from the value passed to Literal's aggregate-style constructor.
    template <typename T>
    Literal(T) -> Literal<T>;

    /// Wraps value as a Literal<T>, deducing T from the argument.
    template <typename T>
    constexpr Literal<std::remove_cvref_t<T>> lit(T&& value) {
        return Literal<std::remove_cvref_t<T>>{std::forward<T>(value)};
    }

    /// @overload
    constexpr Literal<std::string_view> lit(const char* value) {
        return Literal<std::string_view>{value};
    }

    namespace detail {
        /// Passes value through unchanged if it already accepts a visitor (i.e. is already an expression
        /// node); otherwise wraps it in a Literal<T>.
        template <typename T>
        constexpr auto make_literal_if_needed(T&& value) {
            if constexpr (HasAcceptVisitor<T>) {
                return std::forward<T>(value);
            } else if constexpr (std::is_convertible_v<T, const char*>) {
                return Literal<std::string_view>{value};
            } else {
                return Literal<std::remove_cvref_t<T>>{std::forward<T>(value)};
            }
        }

    }  // namespace detail

    /// Marker leaf for an explicit runtime parameter with no literal value attached (a `?`/`$N` placeholder
    /// the caller will bind separately).
    template <typename T>
    struct ParamPlaceholder {
        /// Double-dispatch entry point: forwards *this to visitor.visit(*this).
        constexpr decltype(auto) accept(this auto&& self, auto& visitor);
    };

    /// Tag type for a SQL NULL literal.
    struct NullLiteral {};

    /// The NullLiteral instance comparison operators and .where(...) accept in place of a value.
    constexpr NullLiteral null_value{};


    /// CRTP base adding an `alias` member and `.as(name)` to expression nodes that can appear with a display
    /// alias (SELECT columns, subqueries, CASE expressions, ...).
    template <typename Derived>
    class AliasableExpression : public Expression<Derived> {
    public:
        /// Constructs with an initial display alias.
        template <beavers::IsStringLike StringTp>
        constexpr explicit AliasableExpression(StringTp&& alias)
            : alias{std::forward<StringTp>(alias)} {
        }

        /// Sets the display alias and returns *this (as Derived) for chaining.
        template <typename Self, beavers::IsStringLike StringTp>
        constexpr auto&& as(this Self&& self, StringTp&& name) {
            self.alias = std::forward<StringTp>(name);
            return static_cast<std::conditional_t<std::is_lvalue_reference_v<Self>, Derived&, Derived&&>>(
                std::forward<Self>(self));
        }

        std::string alias;  ///< Display alias; empty when unset.

    protected:
        constexpr AliasableExpression() = default;
    };

    // Feature tags for QueryOperations<Derived, AllowedFeatures...>: a query shape only exposes the fluent
    // method matching a tag present in its AllowedFeatures pack.
    /// Enables .where(...).
    struct AllowWhere {};

    /// Enables .group_by(...).
    struct AllowGroupBy {};

    /// Enables .having(...).
    struct AllowHaving {};

    /// Enables .join(...).
    struct AllowJoin {};

    /// Enables .order_by(...).
    struct AllowOrderBy {};

    /// Enables .limit(...).
    struct AllowLimit {};

    /// Enables SELECT DISTINCT.
    struct AllowDistinct {};

    /// Enables the set-operation operators (|, +, &, -).
    struct AllowUnion {};


    /// True when Feature is one of Features...; used to gate QueryOperations methods on AllowedFeatures.
    template <typename Feature, typename... Features>
    constexpr bool has_feature = (std::is_same_v<Feature, Features> || ...);

    /// Builder returned by `.join(table, type)`; `.on(condition)` completes it into a JoinExpr. Kept as a
    /// separate type (rather than building JoinExpr directly) so the ON condition can be supplied in a second
    /// fluent call.
    template <typename Parent, IsTable TableT>
    class JoinBuilder {
    public:
        /// Captures the parent query, the table being joined, and the join type; awaits `.on(condition)`.
        template <typename P, typename T>
            requires std::same_as<std::remove_cvref_t<P>, Parent> && std::constructible_from<TableT, T>
        constexpr JoinBuilder(P&& parent, T&& right_table, const JoinType type)
            : parent_{std::forward<P>(parent)},
              right_table_name_{std::forward<T>(right_table)},
              type_{type} {
        }

        /// Sets the joined table's alias and returns *this for chaining.
        template <typename Self, beavers::IsStringLike StringTp>
        constexpr auto&& as(this Self&& self, StringTp&& name) {
            self.right_alias_ = std::forward<StringTp>(name);
            return std::forward<Self>(self);
        }

        /// Completes the join with its ON condition, producing the JoinExpr node.
        template <typename Self, IsCondition Condition>
        constexpr auto on(this Self&& self, Condition&& cond) {
            return JoinExpr<Parent, std::remove_cvref_t<Condition>, TableT>{
                std::forward_like<Self>(self.parent_),
                std::forward_like<Self>(self.right_table_name_),
                std::forward<Condition>(cond),
                std::forward_like<Self>(self.type_),
                std::forward_like<Self>(self.right_alias_)};
        }

    private:
        Parent parent_;
        TableT right_table_name_;
        JoinType type_;
        std::string right_alias_;
    };

    /// Owns a single column operand; the common base for the COUNT/SUM/AVG/MIN/MAX aggregate expression
    /// types.
    template <IsColumnLike ColT>
    class ColumnHolder {
    public:
        /// Wraps col as the held column operand.
        template <typename ColumnTp>
            requires(!std::same_as<std::remove_cvref_t<ColumnTp>, ColumnHolder>) &&
                    std::constructible_from<ColT, ColumnTp>
        constexpr explicit ColumnHolder(ColumnTp&& col) noexcept
            : column{std::forward<ColumnTp>(col)} {
        }

        ColT column;  ///< The wrapped column operand.
    };

    /// Owns a single table operand; the common base for the DDL (CREATE/DROP TABLE) expression types.
    template <IsTable TableT>
    class TableHolder {
    public:
        /// Wraps table as the held table operand.
        template <typename TableTp>
            requires(!std::same_as<std::remove_cvref_t<TableTp>, TableHolder>) &&
                    std::constructible_from<TableT, TableTp>
        constexpr explicit TableHolder(TableTp&& table) noexcept
            : table{std::forward<TableTp>(table)} {
        }

        TableT table;  ///< The wrapped table operand.
    };

    /**
     * @brief CRTP mixin adding the fluent .where()/.group_by()/.having()/.join()/.order_by()/.limit() methods
     *        to a query expression node.
     *
     * Each method is constrained by `has_feature<AllowXxx, AllowedFeatures...>`, so calling e.g. `.having(...)`
     * on a query shape whose AllowedFeatures pack does not contain AllowHaving fails to compile rather than
     * misbehaving at runtime; this is how the expression templates encode "which clauses can follow which"
     * (e.g. a bare SELECT cannot .having() before a .group_by()/.where()).
     */
    template <typename Derived, typename... AllowedFeatures>
    class QueryOperations {
    public:
        // WHERE
        /// Narrows to `... WHERE cond`.
        template <typename Self, IsCondition Condition>
            requires(has_feature<AllowWhere, AllowedFeatures...>)
        [[nodiscard]] constexpr auto where(this Self&& self, Condition&& cond) {
            return WhereExpr<Derived, std::remove_cvref_t<Condition>>{std::forward<Self>(self).derived(),
                                                                      std::forward<Condition>(cond)};
        }

        // GROUP BY
        /// Narrows to `... GROUP BY cols...`.
        template <typename Self, IsColumnLike... GroupColumns>
            requires(has_feature<AllowGroupBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto group_by(this Self&& self, GroupColumns&&... cols) {
            return GroupByColumnExpr<Derived, std::remove_cvref_t<GroupColumns>...>{
                std::forward<Self>(self).derived(), std::forward<GroupColumns>(cols)...};
        }

        /// @overload
        template <typename Self, IsQuery GroupingCriteria>
            requires(has_feature<AllowGroupBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto group_by(this Self&& self, GroupingCriteria&& query) {
            return GroupByQueryExpr<Derived, std::remove_cvref_t<GroupingCriteria>>{
                std::forward<Self>(self).derived(), std::forward<GroupingCriteria>(query)};
        }

        // HAVING
        /// Narrows to `... HAVING cond`.
        template <typename Self, IsCondition Condition>
            requires(has_feature<AllowHaving, AllowedFeatures...>)
        [[nodiscard]] constexpr auto having(this Self&& self, Condition&& cond) {
            return HavingExpr<Derived, std::remove_cvref_t<Condition>>{std::forward<Self>(self).derived(),
                                                                       std::forward<Condition>(cond)};
        }

        // JOIN - DynamicTablePtr (shared_ptr-based, runtime)
        /// Starts `... JOIN table ON ...` (type defaults to INNER); returns a JoinBuilder, completed by
        /// `.on(condition)`. This overload takes a runtime DynamicTablePtr.
        template <typename Self, typename TableTp>
            requires(has_feature<AllowJoin, AllowedFeatures...>) &&
                    std::constructible_from<DynamicTablePtr, std::remove_cvref_t<TableTp>>
        [[nodiscard]] constexpr auto join(this Self&& self, TableTp&& table, JoinType type = JoinType::INNER) {
            return JoinBuilder<Derived, DynamicTablePtr>{
                std::forward<Self>(self).derived(), std::forward<TableTp>(table), type};
        }

        // JOIN - IsStringLike && !IsStringViewLike -> std::string
        /// @overload
        template <typename Self, typename TableTp>
            requires(has_feature<AllowJoin, AllowedFeatures...>) && beavers::IsStringLike<TableTp> &&
                    (!beavers::IsStringViewLike<TableTp>)
        [[nodiscard]] constexpr auto join(this Self&& self, TableTp&& table, JoinType type = JoinType::INNER) {
            return JoinBuilder<Derived, std::string>{
                std::forward<Self>(self).derived(), std::forward<TableTp>(table), type};
        }

        // JOIN - const char* -> std::string_view (constexpr-friendly)
        /// @overload
        template <typename Self>
            requires(has_feature<AllowJoin, AllowedFeatures...>)
        [[nodiscard]] constexpr auto join(this Self&& self, const char* table, JoinType type = JoinType::INNER) {
            return JoinBuilder<Derived, std::string_view>{std::forward<Self>(self).derived(), table, type};
        }

        // JOIN - IsStaticTable (compile-time table)
        /// @overload
        template <typename Self, IsStaticTable TableTp>
            requires(has_feature<AllowJoin, AllowedFeatures...>)
        [[nodiscard]] constexpr auto join(this Self&& self, const TableTp& table, JoinType type = JoinType::INNER) {
            return JoinBuilder<Derived, TableTp>{std::forward<Self>(self).derived(), table, type};
        }

        // ORDER BY
        /// Narrows to `... ORDER BY orders...`.
        template <typename Self, IsOrderBy... Orders>
            requires(has_feature<AllowOrderBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto order_by(this Self&& self, Orders&&... orders) {
            return OrderByExpr<Derived, std::remove_cvref_t<Orders>...>{std::forward<Self>(self).derived(),
                                                                        std::forward<Orders>(orders)...};
        }

        // LIMIT
        /// Narrows to `... LIMIT count`.
        template <typename Self, typename T = void>
            requires(has_feature<AllowLimit, AllowedFeatures...>)
        [[nodiscard]] constexpr auto limit(this Self&& self, std::size_t count) {
            return LimitExpr<Derived>{std::forward<Self>(self).derived(), count, 0};
        }

    protected:
        // Helper to get derived instance
        /// Casts *this to Derived&/&&, preserving const and value category.
        [[nodiscard]] constexpr auto&& derived(this auto&& self) {
            return static_cast<std::conditional_t<
                std::is_const_v<std::remove_reference_t<decltype(self)>>,
                std::conditional_t<std::is_lvalue_reference_v<decltype(self)>, const Derived&, const Derived&&>,
                std::conditional_t<std::is_lvalue_reference_v<decltype(self)>, Derived&, Derived&&>>>(
                std::forward<decltype(self)>(self));
        }
    };
}  // namespace menagerie::db
