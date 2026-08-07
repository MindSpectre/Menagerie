#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <menagerie/serialization>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "credentials/postgres_connection_credentials.hpp"
#include "tools/postgres_connection_tools.hpp"
#include "tools/postgres_nodes_role.hpp"
#include "tools/postgres_ssl_mode.hpp"

namespace menagerie::db::postgres {

    /// Transaction isolation levels, mapped onto PostgreSQL's SET TRANSACTION ISOLATION LEVEL.
    enum class TransactionIsolation { READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };

    /**
     * @brief Full PostgreSQL connection configuration
     *
     * Contains ConnectionCredentials plus all advanced options for production deployments
     * including SSL, timeouts, node roles, and performance tuning.
     *
     * Suitable for Session class with LMAX disruptor pool.
     *
     * Usage:
     *   auto config = ConnectionConfig::Builder{}
     *       .host("db.example.com")
     *       .port(5432)
     *       .dbname("production")
     *       .user("app_user")
     *       .password("secret")
     *       .ssl_mode(SslMode::VERIFY_FULL)
     *       .ssl_root_cert("/path/to/ca.pem")
     *       .connect_timeout(std::chrono::seconds{30})
     *       .role(NodeRole::PRIMARY)
     *       .finalize();
     */
    class ConnectionConfig final : public serialization::ConfigInterface<ConnectionConfig, Json::Value> {
    public:
        /// Wraps existing credentials with default SSL, timeout, and node settings.
        constexpr explicit ConnectionConfig(ConnectionCredentials credentials) noexcept
            : credentials_{std::move(credentials)} {
        }

        // -------- ConfigInterface Implementation --------

        /**
         * @brief Validates credentials plus SSL, timeout, and priority settings.
         * @throw std::invalid_argument if the underlying credentials fail validation (see
         *        ConnectionCredentials::validate()), if ssl_root_cert is missing while
         *        ssl_mode is VERIFY_CA or VERIFY_FULL, if exactly one of ssl_cert/ssl_key is
         *        set, if connect_timeout is negative, or if priority is negative.
         */
        constexpr void validate() const override {
            credentials_.validate();

            if ((ssl_mode_ == SslMode::VERIFY_CA || ssl_mode_ == SslMode::VERIFY_FULL) && !ssl_root_cert_.has_value()) {
                throw std::invalid_argument("ssl_root_cert is required when ssl_mode is VERIFY_CA or VERIFY_FULL");
            }

            if (ssl_cert_.has_value() != ssl_key_.has_value()) {
                throw std::invalid_argument("ssl_cert and ssl_key must both be specified or both be omitted");
            }

            if (connect_timeout_.count() < 0) {
                throw std::invalid_argument("connect_timeout cannot be negative");
            }

            if (priority_ < 0) {
                throw std::invalid_argument("priority cannot be negative");
            }
        }

        // -------- Credential Accessors --------

        /// The wrapped host/port/dbname/user/password credentials.
        [[nodiscard]] constexpr const ConnectionCredentials& credentials() const noexcept {
            return credentials_;
        }

        // -------- Getters --------

        // Credential getters (delegated)
        /// Configured host, delegated to credentials().
        [[nodiscard]] constexpr std::string_view host() const noexcept {
            return credentials_.host();
        }
        /// Configured port, delegated to credentials().
        [[nodiscard]] constexpr std::string_view port() const noexcept {
            return credentials_.port();
        }
        /// Configured database name, delegated to credentials().
        [[nodiscard]] constexpr std::string_view dbname() const noexcept {
            return credentials_.dbname();
        }
        /// Configured user name, delegated to credentials().
        [[nodiscard]] constexpr std::string_view user() const noexcept {
            return credentials_.user();
        }
        /// Configured password, delegated to credentials().
        [[nodiscard]] constexpr std::string_view password() const noexcept {
            return credentials_.password();
        }

        // Node configuration
        /// This node's role in a read/write-split-style deployment (default PRIMARY).
        [[nodiscard]] constexpr NodeRole role() const noexcept {
            return role_;
        }
        /// Selection priority among same-role nodes; higher is preferred (default 0).
        [[nodiscard]] constexpr int priority() const noexcept {
            return priority_;
        }
        /// Cluster this node belongs to, for grouping nodes across a deployment.
        [[nodiscard]] constexpr std::string_view cluster_name() const noexcept {
            return cluster_name_;
        }

        // Timeouts
        /// libpq connection establishment timeout (default 30s).
        [[nodiscard]] constexpr std::chrono::seconds connect_timeout() const noexcept {
            return connect_timeout_;
        }
        /// Configured statement_timeout value (0 = no limit); not currently applied by the provider.
        [[nodiscard]] constexpr std::chrono::seconds statement_timeout() const noexcept {
            return statement_timeout_;
        }
        /// Configured idle_in_transaction_session_timeout value (0 = no limit); not currently applied by the provider.
        [[nodiscard]] constexpr std::chrono::seconds idle_in_transaction_timeout() const noexcept {
            return idle_in_transaction_timeout_;
        }
        /// Configured lock_timeout value (0 = no limit); not currently applied by the provider.
        [[nodiscard]] constexpr std::chrono::seconds lock_timeout() const noexcept {
            return lock_timeout_;
        }

        // SSL/TLS
        /// Configured sslmode (default PREFER).
        [[nodiscard]] constexpr SslMode ssl_mode() const noexcept {
            return ssl_mode_;
        }
        /// Client certificate path, if set; must be paired with ssl_key().
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_cert() const noexcept {
            return ssl_cert_;
        }
        /// Client private key path, if set; must be paired with ssl_cert().
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_key() const noexcept {
            return ssl_key_;
        }
        /// Root CA certificate path, required when ssl_mode() is VERIFY_CA or VERIFY_FULL.
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_root_cert() const noexcept {
            return ssl_root_cert_;
        }

        // Protocol
        /// Configured value for whether libpq's binary result/param format is preferred (default
        /// true); not currently applied by the provider.
        [[nodiscard]] constexpr bool binary_protocol() const noexcept {
            return binary_protocol_;
        }
        /// Configured value for whether repeated statements are auto-prepared (default false);
        /// not currently applied by the provider.
        [[nodiscard]] constexpr bool auto_prepare() const noexcept {
            return auto_prepare_;
        }
        /// Configured value for whether libpq pipeline mode is enabled (default true); not
        /// currently applied by the provider.
        [[nodiscard]] constexpr bool pipeline_mode() const noexcept {
            return pipeline_mode_;
        }
        /// application_name reported to the server.
        [[nodiscard]] constexpr std::string_view application_name() const noexcept {
            return application_name_;
        }
        /// Server-side search_path (default "public").
        [[nodiscard]] constexpr std::string_view search_path() const noexcept {
            return search_path_;
        }

        // Performance
        /// Configured work_mem value in megabytes (default 4); not currently applied by the provider.
        [[nodiscard]] constexpr std::size_t work_mem_mb() const noexcept {
            return work_mem_mb_;
        }
        /// Configured value for whether server-side JIT compilation is enabled (default true);
        /// not currently applied by the provider.
        [[nodiscard]] constexpr bool jit() const noexcept {
            return jit_;
        }

        // Extra options
        /// Additional libpq connection-string options as key/value pairs.
        [[nodiscard]] const std::map<std::string, std::string>& extra_options() const noexcept {
            return extra_options_;
        }

        // -------- Connection String --------

        /**
         * @brief Generate full libpq connection string
         * @return Connection string including all configured options
         * @throw std::invalid_argument if the configuration fails validate().
         */
        [[nodiscard]] constexpr std::string to_connection_string() const {
            validate();
            std::string result = credentials_.to_connection_string();

            result += " sslmode=" + ssl_mode_to_string_t<std::string>(ssl_mode_);

            if (ssl_cert_) {
                result += " sslcert=" + detail::escape_connection_value(*ssl_cert_);
            }
            if (ssl_key_) {
                result += " sslkey=" + detail::escape_connection_value(*ssl_key_);
            }
            if (ssl_root_cert_) {
                result += " sslrootcert=" + detail::escape_connection_value(*ssl_root_cert_);
            }

            if (connect_timeout_.count() > 0) {
                result += " connect_timeout=" + std::to_string(connect_timeout_.count());
            }

            if (!application_name_.empty()) {
                result += " application_name=" + detail::escape_connection_value(application_name_);
            }

            for (const auto& [key, value] : extra_options_) {
                result += " " + key + "=" + detail::escape_connection_value(value);
            }

            return result;
        }

        // -------- Factory Methods (defined after Builder) --------

        /// Localhost preset: SSL disabled, short connect_timeout, credentials left default.
        [[nodiscard]] static ConnectionConfig local_dev();
        /// Local test-database preset: port 5433, dedicated test_db/test_user, SSL disabled.
        [[nodiscard]] static ConnectionConfig testing();
        /// Production preset: full certificate verification, longer statement_timeout, JIT and
        /// pipeline mode enabled; credentials must be supplied separately.
        [[nodiscard]] static ConnectionConfig production();

        // -------- Field Descriptors --------

        /// Field descriptors for serialization::ConfigInterface auto-(de)serialization.
        static constexpr auto fields() {
            return std::tuple{
                // Nested config (auto-serializes via HasFields overload)
                serialization::Field<&ConnectionConfig::credentials_, "credentials">{},
                // Node configuration
                serialization::Field<&ConnectionConfig::role_, "role">{},
                serialization::Field<&ConnectionConfig::priority_, "priority">{},
                serialization::Field<&ConnectionConfig::cluster_name_, "cluster_name">{},
                // Timeouts
                serialization::Field<&ConnectionConfig::connect_timeout_, "connect_timeout">{},
                serialization::Field<&ConnectionConfig::statement_timeout_, "statement_timeout">{},
                serialization::Field<&ConnectionConfig::idle_in_transaction_timeout_, "idle_in_transaction_timeout">{},
                serialization::Field<&ConnectionConfig::lock_timeout_, "lock_timeout">{},
                // SSL/TLS
                serialization::Field<&ConnectionConfig::ssl_mode_, "ssl_mode">{},
                serialization::Field<&ConnectionConfig::ssl_cert_, "ssl_cert">{},
                serialization::Field<&ConnectionConfig::ssl_key_, "ssl_key">{},
                serialization::Field<&ConnectionConfig::ssl_root_cert_, "ssl_root_cert">{},
                // Protocol
                serialization::Field<&ConnectionConfig::binary_protocol_, "binary_protocol">{},
                serialization::Field<&ConnectionConfig::auto_prepare_, "auto_prepare">{},
                serialization::Field<&ConnectionConfig::pipeline_mode_, "pipeline_mode">{},
                serialization::Field<&ConnectionConfig::application_name_, "application_name">{},
                serialization::Field<&ConnectionConfig::search_path_, "search_path">{},
                // Performance
                serialization::Field<&ConnectionConfig::work_mem_mb_, "work_mem_mb">{},
                serialization::Field<&ConnectionConfig::jit_, "jit">{},
                // Extra options
                serialization::Field<&ConnectionConfig::extra_options_, "extra_options">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr ConnectionConfig() = default;

        ConnectionCredentials credentials_;

        // Node configuration
        NodeRole role_ = NodeRole::PRIMARY;
        int priority_  = 0;
        std::string cluster_name_;

        // Timeouts
        std::chrono::seconds connect_timeout_             = std::chrono::seconds{30};
        std::chrono::seconds statement_timeout_           = std::chrono::seconds{0};
        std::chrono::seconds idle_in_transaction_timeout_ = std::chrono::seconds{0};
        std::chrono::seconds lock_timeout_                = std::chrono::seconds{0};

        // SSL/TLS
        SslMode ssl_mode_ = SslMode::PREFER;
        std::optional<std::string> ssl_cert_;
        std::optional<std::string> ssl_key_;
        std::optional<std::string> ssl_root_cert_;

        // Protocol settings
        bool binary_protocol_ = true;
        bool auto_prepare_    = false;
        bool pipeline_mode_   = true;
        std::string application_name_;
        std::string search_path_ = "public";

        // Performance
        std::size_t work_mem_mb_ = 4;
        bool jit_                = true;

        // Additional libpq options
        std::map<std::string, std::string> extra_options_;
    };

    /// Fluent builder for ConnectionConfig; finalize() validates and builds.
    class ConnectionConfig::Builder {
    public:
        /// Starts from a default-constructed ConnectionConfig.
        Builder() = default;
        /// Starts pre-populated by copying an existing ConnectionConfig.
        explicit Builder(const ConnectionConfig& existing)
            : config_{existing} {
        }
        /// Starts pre-populated by moving from an existing ConnectionConfig.
        explicit Builder(ConnectionConfig&& existing)
            : config_{std::move(existing)} {
        }

        // -------- Credential Setters (Proxied) --------

        /// Replaces the whole embedded ConnectionCredentials.
        template <typename Self>
        constexpr auto&& credentials(this Self&& self, ConnectionCredentials creds) noexcept {
            self.config_.credentials_ = std::move(creds);
            return std::forward<Self>(self);
        }

        /// Sets the host on the embedded credentials.
        template <typename Self>
        constexpr auto&& host(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::move(value),
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        /// Sets the port (as a string) on the embedded credentials.
        template <typename Self>
        constexpr auto&& port(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        /// Sets the port (as a number) on the embedded credentials.
        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::to_string(value),
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        /// Sets the database name on the embedded credentials.
        template <typename Self>
        constexpr auto&& dbname(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        /// Sets the user name on the embedded credentials.
        template <typename Self>
        constexpr auto&& user(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        /// Sets the password on the embedded credentials.
        template <typename Self>
        constexpr auto&& password(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::move(value)};
            return std::forward<Self>(self);
        }

        // -------- Node Configuration Setters --------

        /// Sets this node's role in a read/write-split-style deployment.
        template <typename Self>
        constexpr auto&& role(this Self&& self, NodeRole value) noexcept {
            self.config_.role_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the selection priority among same-role nodes.
        template <typename Self>
        constexpr auto&& priority(this Self&& self, int value) noexcept {
            self.config_.priority_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the cluster this node belongs to.
        template <typename Self>
        constexpr auto&& cluster_name(this Self&& self, std::string value) noexcept {
            self.config_.cluster_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        // -------- Timeout Setters --------

        /// Sets the libpq connection establishment timeout.
        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the server-side statement_timeout.
        template <typename Self>
        constexpr auto&& statement_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.statement_timeout_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the server-side idle_in_transaction_session_timeout.
        template <typename Self>
        constexpr auto&& idle_in_transaction_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.idle_in_transaction_timeout_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the server-side lock_timeout.
        template <typename Self>
        constexpr auto&& lock_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.lock_timeout_ = value;
            return std::forward<Self>(self);
        }

        // -------- SSL/TLS Setters --------

        /// Sets sslmode.
        template <typename Self>
        constexpr auto&& ssl_mode(this Self&& self, SslMode value) noexcept {
            self.config_.ssl_mode_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the client certificate path; pair with ssl_key().
        template <typename Self>
        constexpr auto&& ssl_cert(this Self&& self, std::string value) noexcept {
            self.config_.ssl_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the client private key path; pair with ssl_cert().
        template <typename Self>
        constexpr auto&& ssl_key(this Self&& self, std::string value) noexcept {
            self.config_.ssl_key_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the root CA certificate path; required for VERIFY_CA/VERIFY_FULL.
        template <typename Self>
        constexpr auto&& ssl_root_cert(this Self&& self, std::string value) noexcept {
            self.config_.ssl_root_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        // -------- Protocol Setters --------

        /// Selects whether libpq's binary result/param format is preferred.
        template <typename Self>
        constexpr auto&& binary_protocol(this Self&& self, bool value) noexcept {
            self.config_.binary_protocol_ = value;
            return std::forward<Self>(self);
        }

        /// Selects whether repeated statements are auto-prepared.
        template <typename Self>
        constexpr auto&& auto_prepare(this Self&& self, bool value) noexcept {
            self.config_.auto_prepare_ = value;
            return std::forward<Self>(self);
        }

        /// Selects whether libpq pipeline mode is enabled.
        template <typename Self>
        constexpr auto&& pipeline_mode(this Self&& self, bool value) noexcept {
            self.config_.pipeline_mode_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the application_name reported to the server.
        template <typename Self>
        constexpr auto&& application_name(this Self&& self, std::string value) noexcept {
            self.config_.application_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the server-side search_path.
        template <typename Self>
        constexpr auto&& search_path(this Self&& self, std::string value) noexcept {
            self.config_.search_path_ = std::move(value);
            return std::forward<Self>(self);
        }

        // -------- Performance Setters --------

        /// Sets the server-side work_mem in megabytes.
        template <typename Self>
        constexpr auto&& work_mem_mb(this Self&& self, std::size_t value) noexcept {
            self.config_.work_mem_mb_ = value;
            return std::forward<Self>(self);
        }

        /// Selects whether server-side JIT compilation is enabled.
        template <typename Self>
        constexpr auto&& jit(this Self&& self, bool value) noexcept {
            self.config_.jit_ = value;
            return std::forward<Self>(self);
        }

        // -------- Extra Options Setters --------

        /// Adds (or overwrites) a single extra libpq connection-string option.
        template <typename Self>
        constexpr auto&& extra_option(this Self&& self, std::string key, std::string value) noexcept {
            self.config_.extra_options_[std::move(key)] = std::move(value);
            return std::forward<Self>(self);
        }

        /// Replaces the whole extra libpq connection-string options map.
        template <typename Self>
        constexpr auto&& extra_options(this Self&& self, std::map<std::string, std::string> options) noexcept {
            self.config_.extra_options_ = std::move(options);
            return std::forward<Self>(self);
        }

        /**
         * @brief Validates the accumulated fields and returns the finished configuration.
         * @throw std::invalid_argument if the configuration fails validate().
         */
        [[nodiscard]] ConnectionConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ConnectionConfig;
        friend class ConfigInterface;
        ConnectionConfig config_;
    };

    // -------- ConnectionConfig Factory Method Definitions --------

    inline ConnectionConfig ConnectionConfig::local_dev() {
        return Builder{}
            .host("localhost")
            .port(static_cast<std::uint16_t>(5432))
            .ssl_mode(SslMode::DISABLE)
            .connect_timeout(std::chrono::seconds{5})
            .finalize();
    }

    inline ConnectionConfig ConnectionConfig::testing() {
        return Builder{}
            .host("localhost")
            .port(static_cast<std::uint16_t>(5433))
            .dbname("test_db")
            .user("test_user")
            .password("test_password")
            .ssl_mode(SslMode::DISABLE)
            .connect_timeout(std::chrono::seconds{10})
            .finalize();
    }

    inline ConnectionConfig ConnectionConfig::production() {
        return Builder{}
            .ssl_mode(SslMode::VERIFY_FULL)
            .connect_timeout(std::chrono::seconds{30})
            .statement_timeout(std::chrono::seconds{60})
            .pipeline_mode(true)
            .jit(true)
            .finalize();
    }

}  // namespace menagerie::db::postgres
