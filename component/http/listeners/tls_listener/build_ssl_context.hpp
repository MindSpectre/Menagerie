#pragma once

#include <string>

#include <boost/asio/ssl/context.hpp>
#include <tls_config.hpp>

namespace menagerie::http {

    /**
     * @brief Build a hardened server ssl::context from cfg.
     *
     * Sets the protocol-version floor, disables legacy protocols/compression,
     * loads cert/key (+ optional passphrase/DH/client-cert verification),
     * configures the session cache, and installs the ALPN select callback.
     *
     * The ALPN callback's `arg` is `&advertised_alpn_wire`, so that STRING MUST
     * OUTLIVE the returned context - TlsListener passes its own member.
     * `advertised_alpn_wire` is the length-prefixed protocol list (e.g.
     * "\x08http/1.1").
     *
     * @throw boost::system::system_error on a cert/key/DH/CA load failure, or
     *     on an OpenSSL cipher-list configuration failure.
     */
    boost::asio::ssl::context build_ssl_context(const TlsConfig& cfg, const std::string& advertised_alpn_wire);

}  // namespace menagerie::http
