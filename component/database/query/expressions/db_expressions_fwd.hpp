#pragma once
#include <concepts>
#include <menagerie/beavers>

#include <db_field_value.hpp>
namespace menagerie::db {

    /**
     * @brief Duck-typed detector used only to probe whether a type exposes an
     *        accept(visitor) member.
     *
     * QueryVisitor is a CRTP template, so no single concrete visitor type can be
     * named in a concept's requires-expression; AcceptDetector stands in for
     * "any visitor-like argument" instead.
     */
    struct AcceptDetector {
        /// No-op sink; only its existence (and callability from accept()) is checked, never its result.
        template <typename T>
        void visit(T&&) {
            beavers::force_non_static(this);
            beavers::force_non_const(this);
        }
    };

    /// Whether T exposes an accept(visitor) member, checked with AcceptDetector
    /// in place of a concrete visitor type.
    template <typename T>
    concept HasAcceptVisitor = requires(std::remove_reference_t<T>& t, AcceptDetector& v) {
        { t.accept(v) };
    } || requires(const std::remove_reference_t<T>& t, AcceptDetector& v) {
        { t.accept(v) };
    };


    struct OpBase;

    /// Whether T is one of the OpXxx tag types (OpEqual, OpAnd, ...) that
    /// parameterize BinaryExpr/UnaryExpr.
    template <typename T>
    concept IsOperator = std::is_base_of_v<OpBase, T>;

    template <class Derived>
    class Expression;

    /// Whether T is a query expression node: every node in the tree derives
    /// from Expression<T> (CRTP).
    template <typename T>
    concept IsQuery = requires {
        // Must inherit from Expression (CRTP pattern)
        requires std::derived_from<std::remove_cvref_t<T>, Expression<std::remove_cvref_t<T>>>;
    };

    template <typename T = FieldValue>
    class Literal;

    /// Whether T is a Literal<...> specialization.
    template <typename T>
    concept IsLiteral = beavers::is_specialization_of_v<std::remove_cvref_t<T>, Literal>;

    template <typename T>
    struct ParamPlaceholder;

    /// Whether T is a ParamPlaceholder<...> specialization.
    template <typename T>
    concept IsParamPlaceholder = beavers::is_specialization_of_v<std::remove_cvref_t<T>, ParamPlaceholder>;

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    /// Whether T is a BinaryExpr<...> specialization.
    template <typename T>
    concept IsBinaryExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, BinaryExpr>;

    template <typename Operand, IsOperator Op>
    class UnaryExpr;

    /// Whether T is a UnaryExpr<...> specialization.
    template <typename T>
    concept IsUnaryExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    /// Whether T is an ExistsExpr<...> specialization.
    template <typename T>
    concept IsExistExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, ExistsExpr>;

    /// Whether T can serve as a BETWEEN bound: an expression node (has
    /// accept()) or a value FieldValue can hold.
    template <typename T>
    concept IsBetweenBound = HasAcceptVisitor<T> || IsFieldValueType<T>;

    // Between expression concept
    template <typename Operand, IsBetweenBound Lower, IsBetweenBound Upper>
    class BetweenExpr;

    /// Whether T is a BetweenExpr<...> specialization.
    template <typename T>
    concept IsBetweenExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, BetweenExpr>;

    // In list expression concept
    template <typename Operand, typename... Values>
    class InListExpr;

    /// Whether T is an InListExpr<...> specialization.
    template <typename T>
    concept IsInListExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, InListExpr>;

    /// Whether T is any condition node usable in a WHERE/HAVING/JOIN ON clause.
    template <typename T>
    concept IsCondition = IsBinaryExpr<T> || IsUnaryExpr<T> || IsExistExpr<T> || IsInListExpr<T> || IsBetweenExpr<T>;

    template <IsColumnLike ColT>
    class CountExpr;

    /// Whether T is a CountExpr<...> specialization.
    template <typename T>
    concept IsCountExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, CountExpr>;

    template <IsColumnLike ColT>
    class SumExpr;

    /// Whether T is a SumExpr<...> specialization.
    template <typename T>
    concept IsSumExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, SumExpr>;

    template <IsColumnLike ColT>
    class AvgExpr;

    /// Whether T is an AvgExpr<...> specialization.
    template <typename T>
    concept IsAvgExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, AvgExpr>;

    template <IsColumnLike ColT>
    class MinExpr;

    /// Whether T is a MinExpr<...> specialization.
    template <typename T>
    concept IsMinExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, MinExpr>;

    template <IsColumnLike ColT>
    class MaxExpr;

    /// Whether T is a MaxExpr<...> specialization.
    template <typename T>
    concept IsMaxExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, MaxExpr>;

    /// Whether T is any of the COUNT/SUM/AVG/MIN/MAX aggregate expression types.
    template <typename T>
    concept IsAggregate = IsCountExpr<T> || IsSumExpr<T> || IsAvgExpr<T> || IsMaxExpr<T> || IsMinExpr<T>;

    /**
     * @brief Concept to check if a type is a valid database operand for comparison/logical operators.
     *
     * This concept ensures that custom operators (==, !=, <, >, &&, ||, etc.) only apply
     * to database expression types and don't interfere with unrelated types (e.g., libpq types).
     * At least one operand must satisfy this concept for the operator overloads to be selected.
     */
    template <typename T>
    concept IsDbOperand = IsColumnLike<T> || IsLiteral<T> || IsParamPlaceholder<T> || IsCondition<T> || IsAggregate<T>;

    // OrderBy expression concept
    template <IsColumnLike ColT>
    class OrderBy;

    /// Whether T is an OrderBy<...> specialization.
    template <typename T>
    concept IsOrderBy = beavers::is_specialization_of_v<std::remove_cvref_t<T>, OrderBy>;

    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause;

    /// Whether T is a WhenClause<...> specialization.
    template <typename T>
    concept IsWhenClause = beavers::is_specialization_of_v<std::remove_cvref_t<T>, WhenClause>;

    // Case expression concepts
    template <IsWhenClause... WhenClauses>
    class CaseExpr;

    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse;

    /// Whether T is a CaseExpr<...> or CaseExprWithElse<...> specialization.
    template <typename T>
    concept IsCaseExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, CaseExpr> ||
                         beavers::is_specialization_of_v<std::remove_cvref_t<T>, CaseExprWithElse>;

    /// Whether T can appear in a SELECT column list: a column, an aggregate, a
    /// CASE expression, or a literal.
    template <typename T>
    concept IsSelectable = IsColumnLike<T> || IsAggregate<T> || IsCaseExpr<T> || IsLiteral<T>;
    // Set operations concept
    template <IsQuery Left, IsQuery Right>
    class SetOpExpr;

    /// Whether T is a SetOpExpr<...> specialization (UNION/INTERSECT/EXCEPT).
    template <typename T>
    concept IsSetOpExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, SetOpExpr>;

    // Subquery concept
    template <IsQuery Query>
    class Subquery;

    /// Whether T is a Subquery<...> specialization.
    template <typename T>
    concept IsSubquery = beavers::is_specialization_of_v<std::remove_cvref_t<T>, Subquery>;

    // Limit expression concept
    template <IsQuery Query>
    class LimitExpr;

    /// Whether T is a LimitExpr<...> specialization.
    template <typename T>
    concept IsLimitExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, LimitExpr>;

    // Group by expression concept
    template <IsQuery Query, IsColumnLike... GroupColumns>
    class GroupByColumnExpr;

    template <IsQuery PreGroupQuery, IsQuery GroupingCriteria>
    class GroupByQueryExpr;

    /// Whether T is a GroupByColumnExpr<...> or GroupByQueryExpr<...>
    /// specialization (GROUP BY a column list vs. GROUP BY a sub-query).
    template <typename T>
    concept IsGroupByExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, GroupByColumnExpr> ||
                            beavers::is_specialization_of_v<std::remove_cvref_t<T>, GroupByQueryExpr>;

    // Having expression concept
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr;

    /// Whether T is a HavingExpr<...> specialization.
    template <typename T>
    concept IsHavingExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, HavingExpr>;

    // Join expression concept
    template <IsQuery Query, IsCondition Condition, IsTable TableT>
    class JoinExpr;

    /// Whether T is a JoinExpr<...> specialization.
    template <typename T>
    concept IsJoinExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, JoinExpr>;

    template <IsQuery Query>
    class CteExpr;

    /// Whether T is a CteExpr<...> specialization (a named WITH clause).
    template <typename T>
    concept IsCteExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, CteExpr>;

    template <IsQuery Query, IsTable TableT>
    class FromTableExpr;

    // From expression concept
    template <IsQuery Query, IsCteExpr CteExpr>
    class FromCteExpr;

    /// Whether T is a FromTableExpr<...> or FromCteExpr<...> specialization
    /// (FROM a table vs. FROM a CTE).
    template <typename T>
    concept IsFromExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, FromTableExpr> ||
                         beavers::is_specialization_of_v<std::remove_cvref_t<T>, FromCteExpr>;


    // Insert expression concepts
    template <IsTable TableT>
    class InsertExpr;

    /// Whether T is an InsertExpr<...> specialization.
    template <typename T>
    concept IsInsertExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, InsertExpr>;

    // Update expression concepts
    template <IsTable TableT>
    class UpdateExpr;

    template <IsTable TableT, IsCondition Condition>
    class UpdateWhereExpr;

    /// Whether T is an UpdateExpr<...> or UpdateWhereExpr<...> specialization
    /// (unconditional UPDATE vs. UPDATE ... WHERE).
    template <typename T>
    concept IsUpdateExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, UpdateExpr> ||
                           beavers::is_specialization_of_v<std::remove_cvref_t<T>, UpdateWhereExpr>;

    // Delete expression concepts
    template <IsTable TableT>
    class DeleteExpr;

    template <IsTable TableT, IsCondition Condition>
    class DeleteWhereExpr;

    /// Whether T is a DeleteExpr<...> or DeleteWhereExpr<...> specialization
    /// (unconditional DELETE vs. DELETE ... WHERE).
    template <typename T>
    concept IsDeleteExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, DeleteExpr> ||
                           beavers::is_specialization_of_v<std::remove_cvref_t<T>, DeleteWhereExpr>;

    template <IsQuery Query, IsCondition Condition>
    class WhereExpr;

    /// Whether T is a WhereExpr<...> specialization.
    template <typename T>
    concept IsWhereExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, WhereExpr>;

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr;

    /// Whether T is an OrderByExpr<...> specialization.
    template <typename T>
    concept IsOrderByExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, OrderByExpr>;

    template <IsSelectable... Columns>
    class SelectExpr;

    /// Whether T is a SelectExpr<...> specialization.
    template <typename T>
    concept IsSelectExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, SelectExpr>;

    // DDL expression concepts
    template <IsTable TableT>
    class CreateTableExpr;

    /// Whether T is a CreateTableExpr<...> specialization.
    template <typename T>
    concept IsCreateTableExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, CreateTableExpr>;

    template <IsTable TableT>
    class DropTableExpr;

    /// Whether T is a DropTableExpr<...> specialization.
    template <typename T>
    concept IsDropTableExpr = beavers::is_specialization_of_v<std::remove_cvref_t<T>, DropTableExpr>;

    /// Whether T is a CREATE TABLE or DROP TABLE expression.
    template <typename T>
    concept IsDdlExpr = IsCreateTableExpr<T> || IsDropTableExpr<T>;
}  // namespace menagerie::db
