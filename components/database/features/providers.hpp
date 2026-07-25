#pragma once
#include <utility>

/// Layered, provider-agnostic PostgreSQL client. base/, primitives/, and
/// query/ describe tables, columns, and SQL expressions in C++ and compile
/// them into parameterized SQL text without knowing which database engine
/// will run it; providers/ (currently PostgreSQL only) supplies the
/// engine-facing half that actually talks to a server. features/ sits below
/// everything else in the dependency graph and carries no internal
/// dependencies of its own - the Providers enum it defines is what dialects,
/// type mappings, and table schemas are all keyed on.
namespace menagerie::db {

    /// Which database backend a schema/dialect/type-mapping is keyed on.
    /// PostgreSQL is compiled in whenever POSTGRESQL_ENABLED is defined (see
    /// BUILD_POSTGRESQL); Redis is declared but not yet wired up by any
    /// build option (REDIS_ENABLED is never defined today).
    enum class Providers {
        None,
#ifdef POSTGRESQL_ENABLED
        PostgreSQL,
#endif
#ifdef REDIS_ENABLED
        Redis
#endif
    };

    /// Renders a Providers value as its enumerator name.
    [[nodiscard]] constexpr const char* to_string(const Providers provider) noexcept {
        switch (provider) {
            case Providers::None:
                return "None";
#ifdef POSTGRESQL_ENABLED
            case Providers::PostgreSQL:
                return "PostgreSQL";
#endif
#ifdef REDIS_ENABLED
            case Providers::Redis:
                return "Redis";
#endif
        }
        std::unreachable();
    }
}  // namespace menagerie::db
