#pragma once
#include <chrono>
#include <string>

#include <boost/exception/all.hpp>
#include <boost/stacktrace/stacktrace.hpp>
namespace menagerie::db {

    // -------- Error Info Tags --------
    // Attach additional context to a database_exception via operator<<; read
    // back with boost::get_error_info<TagType>(e). what() folds every
    // attached tag into the rendered diagnostic message.
    typedef boost::error_info<struct tag_error_code, int> error_code_info;                        ///< Provider-native numeric error code.
    typedef boost::error_info<struct tag_sqlstate, std::string> sqlstate_info;                     ///< SQLSTATE-style error class/code string.
    typedef boost::error_info<struct tag_query, std::string> query_info;                           ///< The SQL text that triggered the error.
    typedef boost::error_info<struct tag_database_name, std::string> database_info;                ///< Target database name.
    typedef boost::error_info<struct tag_host, std::string> host_info;                             ///< Target server host.
    typedef boost::error_info<struct tag_port, int> port_info;                                     ///< Target server port.
    typedef boost::error_info<struct tag_table_name, std::string> table_info;                      ///< Table implicated in the error.
    typedef boost::error_info<struct tag_column_name, std::string> column_info;                    ///< Column implicated in the error.
    typedef boost::error_info<struct tag_constraint_name, std::string> constraint_info;            ///< Name of the violated constraint.
    typedef boost::error_info<struct tag_retry_after, std::chrono::milliseconds> retry_after_info; ///< Suggested backoff before retrying.
    typedef boost::error_info<struct tag_affected_rows, std::size_t> affected_rows_info;           ///< Row count affected before the error.
    typedef boost::error_info<struct tag_error_position, std::size_t> error_position_info;         ///< Byte offset of the error within the query text.
    typedef boost::error_info<struct tag_severity, std::string> severity_info;                     ///< Provider-reported severity level.
    typedef boost::error_info<struct tag_transaction_id, std::string> transaction_id_info;         ///< Identifier of the transaction the error occurred in.

    /**
     * @brief Base of the database exception hierarchy: boost::exception plus
     *        std::exception, with a lazily-cached diagnostic message.
     *
     * what() renders boost::diagnostic_information() on first call and caches
     * the result, so every derived leaf only needs to override category() to
     * describe itself; any of the error-info tags above can be attached via
     * operator<< and they fold into the cached message automatically.
     */
    class database_exception : public boost::exception, public std::exception {
    protected:
        mutable std::string cached_what;  ///< Lazily-computed what() text; empty until the first call.

    public:
        database_exception() noexcept  = default;
        ~database_exception() override = default;

        /// Renders (and caches) the full boost diagnostic message on first call.
        const char* what() const noexcept override {
            if (cached_what.empty()) {
                cached_what = boost::diagnostic_information(*this, false);
            }
            return cached_what.c_str();
        }

        /// Whether retrying the same operation might succeed; false by default.
        virtual bool is_retryable() const noexcept {
            return false;
        }

        /// Dotted category string identifying this exception's place in the
        /// hierarchy (e.g. "server.constraint.unique"), for logging/monitoring.
        [[nodiscard]] constexpr virtual const char* category() const noexcept = 0;
    };

    // -------- Client Side Exceptions --------
    /// Base of every exception representing a problem the caller, not the
    /// server, is responsible for.
    class client_error : public database_exception {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client";
        }
    };

    // Invalid input/arguments
    /// Base for exceptions caused by malformed or invalid caller input.
    class invalid_argument_error : public client_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument";
        }
    };

    /// Malformed SQL text.
    class syntax_error : public invalid_argument_error {
    public:
        syntax_error() = default;
        /// Attaches `msg` as diagnostic info retrievable through what().
        explicit syntax_error(const std::string& msg) {
            *this << boost::error_info<struct tag_message, std::string>(msg);
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.syntax";
        }
    };

    /// A bound parameter's value, count, or shape does not match what the
    /// query expects.
    class invalid_parameter_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.parameter";
        }
    };

    /// A value's C++ type does not match the column or parameter's declared type.
    class type_mismatch_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.type";
        }
    };

    /// A NULL value was read as, or converted to, a non-nullable C++ type.
    class null_conversion_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.null_conversion";
        }
    };

    // Configuration/setup errors
    /// Base for exceptions caused by invalid client-side configuration.
    class configuration_error : public client_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration";
        }
    };

    /// A connection string or its component parameters are malformed.
    class connection_string_error : public configuration_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration.connection_string";
        }
    };

    /// Credentials were missing or rejected by the server.
    class authentication_error : public configuration_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration.auth";
        }
    };

    // -------- Server Side Exceptions --------
    /// Base of every exception representing a problem on the database server side.
    class server_error : public database_exception {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server";
        }
    };

    // Runtime/transient errors
    /// Base for transient server-side errors that may succeed if the
    /// operation is retried.
    class runtime_error : public server_error {
    public:
        bool is_retryable() const noexcept override {
            return true;
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime";
        }
    };

    /// The connection to the server failed or misbehaved.
    class connection_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection";
        }
    };

    /// An established connection was dropped mid-operation.
    class connection_lost_error : public connection_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection.lost";
        }
    };

    /// Establishing or using the connection exceeded its time budget.
    class connection_timeout_error : public connection_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection.timeout";
        }
    };

    /// The server detected a deadlock and aborted the transaction.
    class deadlock_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.deadlock";
        }
    };

    /// The operation timed out waiting to acquire a lock.
    class lock_timeout_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.lock_timeout";
        }
    };

    // Constraint violations
    /// Base for server-side constraint-violation errors; never retryable.
    class constraint_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint";
        }
    };

    /// A UNIQUE constraint rejected the write.
    class unique_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.unique";
        }
    };

    /// A FOREIGN KEY constraint rejected the write.
    class foreign_key_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.foreign_key";
        }
    };

    /// A CHECK constraint rejected the write.
    class check_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.check";
        }
    };

    /// A NOT NULL constraint rejected the write.
    class not_null_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.not_null";
        }
    };

    // Data errors
    /// Base for server-side data-format/range errors; never retryable.
    class data_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data";
        }
    };

    /// A value exceeded the target column's maximum length.
    class data_too_long_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.too_long";
        }
    };

    /// A numeric value overflowed the target column's range.
    class numeric_overflow_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.overflow";
        }
    };

    /// The server rejected a division (or modulo) by zero.
    class division_by_zero_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.division_by_zero";
        }
    };

    // Access/permission errors
    /// Base for server-side permission and lookup errors.
    class access_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access";
        }
    };

    /// The authenticated user lacks permission for the requested operation.
    class permission_denied_error : public access_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access.permission";
        }
    };

    /// The referenced database object (table, column, schema, ...) does not exist.
    class object_not_found_error : public access_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access.not_found";
        }
    };

    // Resource errors
    /// Base for server-resource-exhaustion errors; may succeed if retried
    /// once the resource frees up.
    class resource_error : public server_error {
    public:
        bool is_retryable() const noexcept override {
            return true;
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource";
        }
    };

    /// The server ran out of memory servicing the request.
    class out_of_memory_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.memory";
        }
    };

    /// The server ran out of disk space servicing the request.
    class disk_full_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.disk";
        }
    };

    /// The server rejected the connection because it is at its connection limit.
    class too_many_connections_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.connections";
        }
    };

    // -------- Fatal Exceptions (with stack traces) --------
    /**
     * @brief Base of the never-retryable, unrecoverable exception branch.
     *
     * Captures a stack trace at construction time so the corruption/internal-
     * error site is still visible in logs after the call stack has unwound.
     */
    class fatal_error : public database_exception {
    public:
        fatal_error() {
            stack_trace = boost::stacktrace::to_string(boost::stacktrace::stacktrace());
        }

        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal";
        }

        /// The stack trace captured when this exception was constructed.
        const std::string& get_stack_trace() const noexcept {
            return stack_trace;
        }

    private:
        std::string stack_trace;
    };

    /// An internal invariant of the database driver itself was violated.
    class internal_error : public fatal_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal.internal";
        }
    };

    /// Data corruption was detected.
    class corruption_error : public fatal_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal.corruption";
        }
    };

}  // namespace menagerie::db
