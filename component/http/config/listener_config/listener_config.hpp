#pragma once

#include <cstddef>
#include <cstdint>
#include <menagerie/serialization>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <http_enums.hpp>
#include <tls_config.hpp>

namespace menagerie::http {

    // -- Protocol <-> string codec --
    // Lives HERE, not in http_enums.hpp: the types layer must not grow a
    // serialization/jsoncpp dependency. ADL finds these from the machinery
    // (Protocol's innermost enclosing namespace is menagerie::http).
    /// Converts a Protocol to its wire string ("http1", "http2", "http3").
    [[nodiscard]] constexpr std::string_view to_string_view(const Protocol p) noexcept {
        switch (p) {
            case Protocol::http1:
                return "http1";
            case Protocol::http2:
                return "http2";
            case Protocol::http3:
                return "http3";
        }
        return "http1";
    }

    /// Writes `v`'s wire string into `out[key]`.
    inline void write_field(Json::Value& out, const serialization::FieldName key, const Protocol v) {
        out[key.str()] = std::string{to_string_view(v)};
    }

    /// @throw std::invalid_argument on an unknown protocol string - a silent
    /// fallback would turn a typo into serving the wrong protocol.
    inline bool read_field(const Json::Value& in, const serialization::FieldName key, Protocol& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "http1") {
            v = Protocol::http1;
            return true;
        }
        if (raw == "http2") {
            v = Protocol::http2;
            return true;
        }
        if (raw == "http3") {
            v = Protocol::http3;
            return true;
        }
        throw std::invalid_argument{"config field '" + k + "': unknown protocol '" + raw +
                                    R"(' (expected "http1"|"http2"|"http3"))"};
    }

    /**
     * @brief One listening endpoint: transport + protocol set + optional TLS
     *        material.
     *
     * `protocols` order is meaningful for multi-protocol TLS listeners: it is
     * the ALPN server-preference order (attach_default_listeners maps it onto
     * TlsListener's template-argument order). validate() enforces protocol
     * FACTS only (http3 requires the quic transport and vice versa,
     * TLS-material presence, no duplicates); which combinations the server
     * can actually serve is attach_default_listeners' concern instead - the
     * config layer stays driver-availability-agnostic.
     */
    class ListenerConfig final : public serialization::ConfigInterface<ListenerConfig, Json::Value> {
    public:
        /// Transport the listener accepts connections on.
        enum class Transport : std::uint8_t { tcp, tls, quic };

        constexpr void validate() const override {
            if (bind_address_.empty()) {
                throw std::invalid_argument("listener.bind must not be empty");
            }
            for (std::size_t i = 0; i < protocols_.size(); ++i) {
                for (std::size_t j = i + 1; j < protocols_.size(); ++j) {
                    if (protocols_[i] == protocols_[j]) {
                        throw std::invalid_argument("listener.protocols must not contain duplicates");
                    }
                }
            }
            bool has_h3 = false;
            for (const auto p : protocols_) {
                if (p == Protocol::http3) {
                    has_h3 = true;
                }
            }
            if (transport_ == Transport::quic) {
                if (!protocols_.empty() && (protocols_.size() != 1 || !has_h3)) {
                    throw std::invalid_argument(R"(listener.protocols: quic transport carries exactly ["http3"])");
                }
            } else if (has_h3) {
                throw std::invalid_argument("listener.protocols: http3 requires the quic transport");
            }
            if (transport_ == Transport::tcp) {
                if (tls_.has_value()) {
                    throw std::invalid_argument("listener.tls: only valid for tls/quic transports");
                }
            } else {
                if (!tls_.has_value()) {
                    throw std::invalid_argument("listener.tls: required for tls/quic transports");
                }
                tls_->validate();
            }
        }

        /// The configured bind address.
        [[nodiscard]] const std::string& bind_address() const noexcept {
            return bind_address_;
        }
        /// The configured listen port.
        [[nodiscard]] constexpr std::uint16_t port() const noexcept {
            return port_;
        }
        /// The configured transport (tcp/tls/quic).
        [[nodiscard]] constexpr Transport transport() const noexcept {
            return transport_;
        }
        /// The configured protocol list; empty means the transport's default applies.
        [[nodiscard]] const std::vector<Protocol>& protocols() const noexcept {
            return protocols_;
        }
        /// The configured TLS material, if this listener requires TLS.
        [[nodiscard]] const std::optional<TlsConfig>& tls() const noexcept {
            return tls_;
        }

        /// protocols() with the transport's default filled in (empty => http1;
        /// http3 on quic) - the shape attach_default_listeners consumes.
        [[nodiscard]] std::vector<Protocol> effective_protocols() const {
            if (!protocols_.empty()) {
                return protocols_;
            }
            return {transport_ == Transport::quic ? Protocol::http3 : Protocol::http1};
        }

        /// Field descriptors consumed by the JSON (de)serialization machinery.
        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ListenerConfig::bind_address_, "bind">{},
                serialization::Field<&ListenerConfig::port_, "port">{},
                serialization::Field<&ListenerConfig::transport_, "transport">{},
                serialization::Field<&ListenerConfig::protocols_, "protocols">{},
                serialization::Field<&ListenerConfig::tls_, "tls">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        ListenerConfig() = default;

        std::string bind_address_ = "0.0.0.0";
        std::uint16_t port_       = 8080;
        Transport transport_      = Transport::tcp;
        std::vector<Protocol> protocols_{};
        std::optional<TlsConfig> tls_{};
    };

    // -- Transport <-> string codec --

    /// Converts a Transport to its wire string ("tcp", "tls", "quic").
    [[nodiscard]] constexpr std::string_view to_string_view(const ListenerConfig::Transport t) noexcept {
        switch (t) {
            case ListenerConfig::Transport::tcp:
                return "tcp";
            case ListenerConfig::Transport::tls:
                return "tls";
            case ListenerConfig::Transport::quic:
                return "quic";
        }
        return "tcp";
    }

    /// Writes `v`'s wire string into `out[key]`.
    inline void write_field(Json::Value& out, const serialization::FieldName key, const ListenerConfig::Transport v) {
        out[key.str()] = std::string{to_string_view(v)};
    }

    /// @throw std::invalid_argument on an unknown transport string - a silent
    /// fallback would turn a typo into binding the wrong transport.
    inline bool read_field(const Json::Value& in, const serialization::FieldName key, ListenerConfig::Transport& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "tcp") {
            v = ListenerConfig::Transport::tcp;
            return true;
        }
        if (raw == "tls") {
            v = ListenerConfig::Transport::tls;
            return true;
        }
        if (raw == "quic") {
            v = ListenerConfig::Transport::quic;
            return true;
        }
        throw std::invalid_argument{"config field '" + k + "': unknown transport '" + raw +
                                    R"(' (expected "tcp"|"tls"|"quic"))"};
    }

    /**
     * @brief Fluent builder for ListenerConfig.
     *
     * Chainable setters (`this Self&&`) work on both lvalue and rvalue
     * builders; finalize() validates and returns the built config.
     */
    class ListenerConfig::Builder {
    public:
        Builder() = default;

        /// Sets the bind address.
        template <typename Self>
        constexpr auto&& bind_address(this Self&& self, std::string value) noexcept {
            self.config_.bind_address_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the listen port.
        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.port_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the transport (tcp/tls/quic).
        template <typename Self>
        constexpr auto&& transport(this Self&& self, const Transport value) noexcept {
            self.config_.transport_ = value;
            return std::forward<Self>(self);
        }

        /// Sets the protocol list.
        template <typename Self>
        constexpr auto&& protocols(this Self&& self, std::vector<Protocol> value) noexcept {
            self.config_.protocols_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Sets the TLS material.
        template <typename Self>
        constexpr auto&& tls(this Self&& self, TlsConfig value) noexcept {
            self.config_.tls_ = std::move(value);
            return std::forward<Self>(self);
        }

        /// Validates and returns the built ListenerConfig.
        [[nodiscard]] constexpr ListenerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ListenerConfig;
        friend class ConfigInterface;
        ListenerConfig config_;
    };

}  // namespace menagerie::http
