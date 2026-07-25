#pragma once

namespace menagerie::http {

    class Server;

    /**
     * @brief Walk server.config().listeners() and add the matching listener +
     *        driver instances - the config-driven wiring path.
     *
     * Driver config derives from the server-level config: Http11Config gets
     * body_limit + the three phase timeouts (max_header_bytes keeps its 16 KB
     * struct default - no ServerConfig field maps to it currently).
     *
     * Currently supported (transport, protocols) combinations:
     *   tcp  + [http1]                        -> TcpListener<Http11Driver>
     *   tls  + [http1]                        -> TlsListener<Http11Driver>
     *   tls  + [http2]                        -> TlsListener<Http2Driver> (scaffold driver)
     *   tls  + [http1, http2] (either order)  -> TlsListener<...> in the LISTED
     *                                            order - JSON order is the ALPN
     *                                            server-preference order
     *   quic + [http3]                        -> QuicListener<Http3Driver> (scaffold)
     * An empty protocols list defaults per transport (http1; http3 on quic).
     *
     * Call during the build phase (before setup()). An empty listeners array
     * is a no-op - programmatic add_*_listener calls compose with this.
     *
     * @throw std::invalid_argument on a combination that cannot be served
     *         (e.g. tcp+[http2] - h2c is not supported).
     */
    void attach_default_listeners(Server& server);

}  // namespace menagerie::http
