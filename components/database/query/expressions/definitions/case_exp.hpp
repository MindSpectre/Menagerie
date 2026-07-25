#pragma once

#include "basic.hpp"

namespace menagerie::db {
    /// A single `WHEN condition THEN value` pair inside a CASE expression.
    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause {
        ConditionExpr condition;  ///< The WHEN condition.
        ValueExpr value;          ///< The THEN value (wrapped in a Literal if it was not already a node).

        /// Wraps cond/val as the clause's condition and value.
        template <typename ConditionExprTp, typename ValueExprTp>
            requires std::constructible_from<ConditionExpr, ConditionExprTp> &&
                         std::constructible_from<ValueExpr, ValueExprTp>
        constexpr WhenClause(ConditionExprTp&& cond, ValueExprTp&& val) noexcept
            : condition{std::forward<ConditionExprTp>(cond)},
              value{std::forward<ValueExprTp>(val)} {
        }

        /// Splits *this into {condition, value} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.condition), std::forward_like<Self>(self.value));
        }
    };

    // Base class with common functionality
    /**
     * @brief CRTP base shared by CaseExpr and CaseExprWithElse: owns the WHEN-clause tuple and implements
     *        .when(condition, value).
     *
     * Each .when(...) call appends a clause, which changes the WHEN-clause pack and therefore the concrete
     * type - `.when()` returns a new, larger CaseExpr/CaseExprWithElse rather than mutating in place. Derived
     * supplies add_when_clause(...) to construct that larger type with the right derived class (CaseExpr vs.
     * CaseExprWithElse).
     */
    template <IsCaseExpr Derived, IsWhenClause... WhenClauses>
    class CaseExprBase : public AliasableExpression<Derived> {
    public:
        /// Constructs directly from an already-built clauses tuple (used by add_when_clause()).
        template <typename TupleTp>
            requires std::constructible_from<std::tuple<WhenClauses...>, TupleTp>
        constexpr explicit CaseExprBase(TupleTp&& clauses) noexcept
            : when_clauses_{std::forward<TupleTp>(clauses)} {
        }

        /// @overload
        template <typename... WhenClausesTp>
            requires(std::constructible_from<WhenClauses, WhenClausesTp> && ...)
        constexpr explicit CaseExprBase(WhenClausesTp&&... clauses) noexcept
            : when_clauses_{std::forward<WhenClausesTp>(clauses)...} {
        }

        /// The WHEN clauses, as a tuple.
        template <typename Self>
        [[nodiscard]] constexpr auto&& when_clauses(this Self&& self) noexcept {
            return std::forward_like<Self>(self.when_clauses_);
        }

        /// Appends `WHEN condition THEN value` (wrapping value in a Literal if it is not already an
        /// expression node) and returns the resulting, larger CaseExpr/CaseExprWithElse.
        template <typename Self, typename ConditionExpr, typename ValueExpr>
        [[nodiscard]] constexpr auto when(this Self&& self, ConditionExpr&& condition, ValueExpr&& value) {
            auto wrapped_value  = detail::make_literal_if_needed(std::forward<ValueExpr>(value));
            using NewWhenClause = WhenClause<std::decay_t<ConditionExpr>, decltype(wrapped_value)>;

            return std::forward<Self>(self).add_when_clause(
                NewWhenClause(std::forward<ConditionExpr>(condition), std::move(wrapped_value)));
        }

    protected:
        std::tuple<WhenClauses...> when_clauses_;  ///< The accumulated WHEN clauses, in order.
    };

    // Case expression without else clause
    /// `CASE WHEN ... THEN ... END`, without an ELSE branch. `.else_(value)` completes it into a
    /// CaseExprWithElse.
    template <IsWhenClause... WhenClauses>
    class CaseExpr : public CaseExprBase<CaseExpr<WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExpr, WhenClauses...>;

    public:
        using Base::Base;  // Inherit constructors

        /// Adds the ELSE branch, wrapping else_expr in a Literal if needed, producing a CaseExprWithElse.
        template <typename Self, typename ElseExpr>
        [[nodiscard]] constexpr auto else_(this Self&& self, ElseExpr&& else_expr) {
            // Auto-wrap the else expression if needed
            auto wrapped_else = detail::make_literal_if_needed(std::forward<ElseExpr>(else_expr));
            return CaseExprWithElse<decltype(wrapped_else), WhenClauses...>(std::forward_like<Self>(self.when_clauses_),
                                                                            std::move(wrapped_else));
        }

        /// Splits *this into {when_clauses, alias} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.when_clauses_),
                                         std::forward_like<Self>(self.alias));
        }

        // Used by base class when() method
        /// Appends new_clause and returns the resulting CaseExpr<WhenClauses..., NewWhenClause>.
        template <typename Self, IsWhenClause NewWhenClause>
        [[nodiscard]] constexpr auto add_when_clause(this Self&& self, NewWhenClause&& new_clause) {
            return CaseExpr<WhenClauses..., std::decay_t<NewWhenClause>>(std::tuple_cat(
                std::forward_like<Self>(self.when_clauses_), std::make_tuple(std::forward<NewWhenClause>(new_clause))));
        }
    };

    // Case expression with else clause
    /// `CASE WHEN ... THEN ... ELSE ... END`.
    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse : public CaseExprBase<CaseExprWithElse<ElseExpr, WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExprWithElse, WhenClauses...>;

    public:
        /// Wraps when_clauses as the accumulated WHEN clauses and else_expr as the ELSE value.
        template <typename TupleTp, typename ElseExprTp>
            requires std::constructible_from<std::tuple<WhenClauses...>, TupleTp> &&
                         std::constructible_from<ElseExpr, ElseExprTp>
        constexpr CaseExprWithElse(TupleTp&& when_clauses, ElseExprTp&& else_expr) noexcept
            : Base{std::forward<TupleTp>(when_clauses)},
              else_clause_{std::forward<ElseExprTp>(else_expr)} {
        }

        /// The ELSE value.
        template <typename Self>
        [[nodiscard]] constexpr auto&& else_clause(this Self&& self) noexcept {
            return std::forward_like<Self>(self.else_clause_);
        }

        /// Splits *this into {when_clauses, else_clause, alias} for the visitor.
        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.when_clauses_),
                                         std::forward_like<Self>(self.else_clause_),
                                         std::forward_like<Self>(self.alias));
        }

        // Used by base class when() method
        /// Appends new_clause and returns the resulting CaseExprWithElse<ElseExpr, WhenClauses...,
        /// NewWhenClause>.
        template <typename Self, IsWhenClause NewWhenClause>
        [[nodiscard]] constexpr auto add_when_clause(this Self&& self, NewWhenClause&& new_clause) {
            return CaseExprWithElse<ElseExpr, WhenClauses..., std::decay_t<NewWhenClause>>(
                std::tuple_cat(std::forward_like<Self>(self.when_clauses_),
                               std::make_tuple(std::forward<NewWhenClause>(new_clause))),
                std::forward_like<Self>(self.else_clause_));
        }

    private:
        ElseExpr else_clause_;
    };

    // Factory function
    /// Starts a CASE expression with one `WHEN condition THEN value` clause (wrapping value in a Literal if
    /// needed); chain further `.when(...)` calls and optionally end with `.else_(...)`.
    template <IsCondition ConditionExpr, typename ValueExpr>
    [[nodiscard]] constexpr auto case_when(ConditionExpr&& condition, ValueExpr&& value) {
        // Auto-wrap the value if needed
        auto wrapped_value = detail::make_literal_if_needed(std::forward<ValueExpr>(value));
        using WhenType     = WhenClause<std::decay_t<ConditionExpr>, decltype(wrapped_value)>;

        return CaseExpr<WhenType>(WhenType(std::forward<ConditionExpr>(condition), std::move(wrapped_value)));
    }

}  // namespace menagerie::db
