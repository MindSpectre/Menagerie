#pragma once

namespace menagerie::db::postgres {
    /// A node's role within a read/write-split-style PostgreSQL deployment.
    enum class NodeRole {
        PRIMARY,        ///< Read/write node.
        STANDBY_SYNC,   ///< Synchronous standby.
        STANDBY_ASYNC,  ///< Asynchronous standby.
        ANALYTICS,      ///< Read-only, for analytics workloads.
        ARCHIVE         ///< Read-only, for historical data.
    };

    /// Converts a NodeRole to its lowercase string form (e.g. for config serialization).
    [[nodiscard]] constexpr std::string_view node_role_to_string(const NodeRole role) noexcept {
        switch (role) {
            case NodeRole::PRIMARY:
                return "primary";
            case NodeRole::STANDBY_SYNC:
                return "standby_sync";
            case NodeRole::STANDBY_ASYNC:
                return "standby_async";
            case NodeRole::ANALYTICS:
                return "analytics";
            case NodeRole::ARCHIVE:
                return "archive";
        }
        std::unreachable();
    }

    /// Converts an integral value to a NodeRole (0-4 as declared above); out-of-range values
    /// fall back to PRIMARY.
    template <typename T>
        requires std::is_integral_v<T>
    [[nodiscard]] constexpr static NodeRole node_role_from_int(const T value) noexcept {
        switch (value) {
            case 0:
                return NodeRole::PRIMARY;
            case 1:
                return NodeRole::STANDBY_SYNC;
            case 2:
                return NodeRole::STANDBY_ASYNC;
            case 3:
                return NodeRole::ANALYTICS;
            case 4:
                return NodeRole::ARCHIVE;
            default:
                return NodeRole::PRIMARY;
        }
        std::unreachable();
    }


}  // namespace menagerie::db::postgres
