#pragma once

#include <menagerie/crow>

#include "compiled_query/compiled_dynamic_query.hpp"
#include "compiled_query/compiled_static_query.hpp"
#include "param_mode.hpp"
#include "query_visitor/sql_generator_visitor.hpp"

namespace menagerie::db {

    /**
     * @brief Turns an expression tree into SQL, in either a compile-time or a runtime path.
     *
     * compile_static(...) writes into a fixed-capacity beavers::InlineString, allocates nothing, and can run
     * in a constexpr context when the expression itself is constexpr; compile_dynamic(...) allocates a
     * std::pmr::monotonic_buffer_resource arena and writes into a std::pmr::string for expressions whose
     * shape or values are only known at runtime. DefaultMode picks each path's default ParamMode, with
     * Sink/Tuple swapped between the two paths when DefaultMode does not apply to that path (Sink has no
     * meaning for the constexpr path, since there is no ParamSink at compile time; Tuple cannot hold a
     * runtime-sized parameter list).
     */
    template <IsSqlDialect DialectT, ParamMode DefaultMode = ParamMode::Tuple>
    class QueryCompiler {
        // Auto-resolve: Sink->Tuple for CT, Tuple->Sink for RT
        static constexpr ParamMode StaticDefault  = (DefaultMode == ParamMode::Sink) ? ParamMode::Tuple : DefaultMode;
        static constexpr ParamMode RuntimeDefault = (DefaultMode == ParamMode::Tuple) ? ParamMode::Sink : DefaultMode;

    public:
        constexpr QueryCompiler() = default;

        // Constexpr path -- InlineString, no heap allocation, true static constexpr
        /// Compiles expr into a CompiledStaticQuery, formatting literals inline (Mode::Inline) or collecting
        /// them into a std::tuple (Mode::Tuple); MaxLen bounds the generated SQL text's capacity.
        template <ParamMode Mode = StaticDefault, std::size_t MaxLen = 1024, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        [[nodiscard]] constexpr auto compile_static(const Expression<Expr>& expr) const {
            SqlGeneratorVisitor<DialectT, beavers::InlineString<MaxLen>, Mode> visitor{};
            auto params = expr.accept(visitor);
            auto sql    = std::move(visitor).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        /// @overload
        template <ParamMode Mode = StaticDefault, std::size_t MaxLen = 1024, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        [[nodiscard]] constexpr auto compile_static(Expression<Expr>&& expr) const {
            SqlGeneratorVisitor<DialectT, beavers::InlineString<MaxLen>, Mode> visitor{};
            auto params = std::move(expr).accept(visitor);
            auto sql    = std::move(visitor).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        // Runtime path -- full compilation with PMR and params
        /// Compiles expr into a CompiledDynamicQuery backed by a fresh arena, formatting literals inline
        /// (Mode::Inline) or pushing them into the dialect's ParamSink (Mode::Sink).
        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        [[nodiscard]] CompiledDynamicQuery compile_dynamic(const Expression<Expr>& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string> visitor{arena.get()};
                expr.accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << CROW_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else if constexpr (Mode == ParamMode::Sink) {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> visitor{bind_packet.sink.get(),
                                                                                         arena.get()};
                expr.accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << CROW_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
            std::unreachable();
        }

        /// @overload
        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        [[nodiscard]] CompiledDynamicQuery compile_dynamic(Expression<Expr>&& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string> visitor{arena.get()};
                std::move(expr).accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << CROW_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else if constexpr (Mode == ParamMode::Sink) {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> visitor{bind_packet.sink.get(),
                                                                                         arena.get()};
                std::move(expr).accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << CROW_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
            std::unreachable();
        }

    private:
        CROW_COMPONENT_PREFIX("QueryCompiler");
    };
}  // namespace menagerie::db
