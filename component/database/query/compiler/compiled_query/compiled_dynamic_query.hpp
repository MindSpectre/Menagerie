#pragma once
#include <cassert>
#include <memory>
#include <memory_resource>

#include <providers.hpp>

namespace menagerie::db {
    /**
     * @brief Result of QueryCompiler::compile_dynamic(...): SQL text plus its runtime parameter set, both
     *        owned by a shared arena.
     *
     * sql_ and backend_packet_ (the dialect-specific bound-parameter block, e.g. a libpq value/length/format
     * triple) are allocated from arena_, so the arena is kept alive here via shared_ptr for as long as either
     * is referenced, even after the compiler and the visitor that produced them are gone.
     */
    class CompiledDynamicQuery {
    public:
        /// Takes ownership of the already-rendered sql, backend_packet and provider, keeping arena alive
        /// alongside them.
        constexpr CompiledDynamicQuery(std::pmr::string sql,
                                       std::shared_ptr<void> backend_packet,
                                       const Providers provider,
                                       std::shared_ptr<std::pmr::monotonic_buffer_resource> arena)
            : sql_{std::move(sql)},
              backend_packet_{std::move(backend_packet)},
              provider_{provider},
              arena_{std::move(arena)} {
        }

        /// The compiled SQL text.
        [[nodiscard]] constexpr std::string_view sql() const& noexcept {
            return sql_;
        }

        /// The compiled SQL text as a NUL-terminated C string.
        [[nodiscard]] constexpr const char* c_sql() const& noexcept {
            return sql_.c_str();
        }

        /// The type-erased, dialect-specific bound-parameter block (null in ParamMode::Inline, where values
        /// are formatted directly into the SQL text instead).
        [[nodiscard]] constexpr std::shared_ptr<void> backend_packet() const& noexcept {
            return backend_packet_;
        }

        /// @pre The caller must ensure T matches the type originally stored in the backend packet.
        /// @warning UB if wrong type
        template <typename T>
        [[nodiscard]] constexpr std::shared_ptr<T> backend_packet_as() const& noexcept {
            return std::static_pointer_cast<T>(backend_packet_);
        }

        /// The database backend/dialect this query was compiled for.
        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        /// The arena sql_ and backend_packet_ were allocated from; kept alive for as long as this query is.
        [[nodiscard]] constexpr std::shared_ptr<std::pmr::monotonic_buffer_resource> arena() const& noexcept {
            return arena_;
        }

    private:
        std::pmr::string sql_;
        std::shared_ptr<void> backend_packet_;
        Providers provider_;
        std::shared_ptr<std::pmr::monotonic_buffer_resource> arena_;  // lifetime keeper
    };
}  // namespace menagerie::db
