#pragma once

#include <memory>

#include <db_field_value.hpp>

namespace menagerie::db {

    /**
     * @brief The interface a dialect's runtime parameter binder implements.
     *
     * A ParamSink accumulates FieldValue arguments in binding order; each
     * push() returns the value's 1-based placeholder index so callers can
     * build parameter references (e.g. "$1", "$2") alongside the push calls.
     */
    class ParamSink {
    public:
        virtual ~ParamSink() noexcept = default;

        /// Appends a copy of `v`, returning its 1-based placeholder index.
        virtual std::size_t push(const FieldValue& v) = 0;

        /// Appends `v` by move, returning its 1-based placeholder index.
        virtual std::size_t push(FieldValue&& v) = 0;

        /// Backend-native packet handle for sinks that build one incrementally;
        /// null for sinks that do not use this mechanism.
        [[nodiscard]] virtual std::shared_ptr<void> packet() const noexcept = 0;
    };

    /// A dialect-produced ParamSink paired with the backend-native packet
    /// handle it has been accumulating into.
    struct DialectBindPacket {
        std::unique_ptr<ParamSink> sink;  ///< The sink that accumulated the bound parameters.
        std::shared_ptr<void> packet;     ///< The backend-native packet handle `sink` built.
    };
}  // namespace menagerie::db
