#pragma once

#include <cstdint>
#include <menagerie/beavers>
#include <menagerie/serialization>
#include <string>
#include <string_view>

#include "tools/postgres_connection_tools.hpp"

namespace menagerie::db::postgres {

    class ConnectionConfig;

    /**
     * @brief Minimal PostgreSQL connection credentials
     *
     * Contains only the essential parameters needed to establish a database connection.
     * Suitable for simple executor creation.
     *
     * Usage:
     *   auto creds = ConnectionCredentials::Builder{}
     *       .host("localhost")
     *       .port(5432)
     *       .dbname("mydb")
     *       .user("admin")
     *       .password("secret")
     *       .finalize();
     *
     *   PGconn* conn = PQconnectdb(creds.to_connection_string().c_str());
     */
    class ConnectionCredentials final : public serialization::ConfigInterface<ConnectionCredentials, Json::Value> {
    public:
        /// Constructs credentials directly from host, port, dbname, user, and password.
        template <beavers::IsStringLike StringTp1,
                  beavers::IsStringLike StringTp2,
                  beavers::IsStringLike StringTp3,
                  beavers::IsStringLike StringTp4,
                  beavers::IsStringLike StringTp5>
        constexpr ConnectionCredentials(
            StringTp1&& host, StringTp2&& port, StringTp3&& dbname, StringTp4&& user, StringTp5&& password)
            : host_{std::forward<StringTp1>(host)},
              port_{std::forward<StringTp2>(port)},
              dbname_{std::forward<StringTp3>(dbname)},
              user_{std::forward<StringTp4>(user)},
              password_{std::forward<StringTp5>(password)} {
        }

        // -------- ConfigInterface Implementation --------

        /**
         * @brief Checks that dbname and user are set and that port, if given, is valid.
         * @throw std::invalid_argument if dbname or user is empty, if port is set but not
         *        parseable as an integer, or if the parsed port is outside [1, 65535].
         */
        constexpr void validate() const override {
            if (dbname_.empty()) {
                throw std::invalid_argument("Database name (dbname) must be specified");
            }
            if (user_.empty()) {
                throw std::invalid_argument("User must be specified");
            }

            if (!port_.empty()) {
                try {
                    if (const int p = std::stoi(port_); p < 1 || p > 65535) {
                        throw std::invalid_argument("Port must be between 1 and 65535");
                    }
                } catch (const std::invalid_argument&) {
                    throw;
                } catch (const std::exception&) {
                    throw std::invalid_argument("Invalid port number");
                }
            }
        }

        // -------- Getters --------

        /// Configured host (default "localhost").
        [[nodiscard]] constexpr std::string_view host() const noexcept {
            return host_;
        }
        /// Configured port as a string (default "5432"); empty defers the choice to libpq.
        [[nodiscard]] constexpr std::string_view port() const noexcept {
            return port_;
        }
        /// Configured database name.
        [[nodiscard]] constexpr std::string_view dbname() const noexcept {
            return dbname_;
        }
        /// Configured user name.
        [[nodiscard]] constexpr std::string_view user() const noexcept {
            return user_;
        }
        /// Configured password.
        [[nodiscard]] constexpr std::string_view password() const noexcept {
            return password_;
        }

        // -------- Connection String --------

        /**
         * @brief Generate libpq connection string
         * @return Connection string in "key=value key=value" format
         * @throw std::invalid_argument if the credentials fail validate().
         */
        [[nodiscard]] constexpr std::string to_connection_string() const {
            validate();
            std::string result;
            result.reserve(256);

            if (!host_.empty()) {
                result += "host=" + detail::escape_connection_value(host_) + " ";
            }
            if (!port_.empty()) {
                result += "port=" + detail::escape_connection_value(port_) + " ";
            }
            if (!dbname_.empty()) {
                result += "dbname=" + detail::escape_connection_value(dbname_) + " ";
            }
            if (!user_.empty()) {
                result += "user=" + detail::escape_connection_value(user_) + " ";
            }
            if (!password_.empty()) {
                result += "password=" + detail::escape_connection_value(password_) + " ";
            }

            // Remove trailing space
            if (!result.empty() && result.back() == ' ') {
                result.pop_back();
            }

            return result;
        }

        // -------- Equality --------

        /// Compares all fields, including password, for equality.
        [[nodiscard]] bool operator==(const ConnectionCredentials& other) const noexcept {
            return host_ == other.host_ && port_ == other.port_ && dbname_ == other.dbname_ && user_ == other.user_ &&
                   password_ == other.password_;
        }

        // -------- Field Descriptors --------

        /// Field descriptors for serialization::ConfigInterface auto-(de)serialization.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ConnectionCredentials::host_, "host">{},
                serialization::Field<&ConnectionCredentials::port_, "port">{},
                serialization::Field<&ConnectionCredentials::dbname_, "dbname">{},
                serialization::Field<&ConnectionCredentials::user_, "user">{},
                serialization::
                    Field<&ConnectionCredentials::password_, "password", serialization::FieldPolicy::Secret>{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        friend class ConnectionConfig;
        constexpr ConnectionCredentials() = default;

        std::string host_ = "localhost";
        std::string port_ = "5432";
        std::string dbname_;
        std::string user_;
        std::string password_;
    };

    /// Fluent builder for ConnectionCredentials; finalize() validates and builds.
    class ConnectionCredentials::Builder {
    public:
        /// Starts from a default-constructed ConnectionCredentials.
        Builder() = default;
        /// Starts pre-populated by copying an existing ConnectionCredentials.
        explicit Builder(const ConnectionCredentials& existing)
            : config_{existing} {
        }
        /// Starts pre-populated by moving from an existing ConnectionCredentials.
        explicit Builder(ConnectionCredentials&& existing)
            : config_{std::move(existing)} {
        }

        /// Sets the host.
        template <typename Self>
        constexpr auto&& host(this Self&& self, std::string value) noexcept {
            self.config_.host_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the port from a string.
        template <typename Self>
        constexpr auto&& port(this Self&& self, std::string value) noexcept {
            self.config_.port_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the port from a numeric value.
        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.port_ = std::to_string(value);
            return std::forward<Self>(self);
        }

        /// Sets the database name.
        template <typename Self>
        constexpr auto&& dbname(this Self&& self, std::string value) noexcept {
            self.config_.dbname_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the user name.
        template <typename Self>
        constexpr auto&& user(this Self&& self, std::string value) noexcept {
            self.config_.user_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the password.
        template <typename Self>
        constexpr auto&& password(this Self&& self, std::string value) noexcept {
            self.config_.password_ = std::move(value);
            return std::forward<Self>(self);
        }

        /**
         * @brief Validates the accumulated fields and returns the finished credentials.
         * @throw std::invalid_argument if the credentials fail validate().
         */
        [[nodiscard]] ConnectionCredentials finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ConnectionCredentials;
        friend class ConfigInterface;
        ConnectionCredentials config_;
    };

}  // namespace menagerie::db::postgres
