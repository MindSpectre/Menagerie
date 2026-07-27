#pragma once

#include <menagerie/crow>
#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace menagerie::http {

    /// HTTP/3 driver - SCAFFOLD. Unimplemented placeholder: it satisfies
    /// IsHttpDriver, but serve() just logs and closes the connection; the
    /// real ngtcp2/nghttp3-backed implementation lands in a future change,
    /// paired with QuicConnection for the actual QUIC transport.
    class Http3Driver {
    public:
        /// Protocol tag this driver serves (IsHttpDriver).
        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http3;
        }

        /// ALPN protocol IDs this driver accepts (IsHttpDriver).
        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"h3"};
            return ALPNS;
        }

        /// Scaffold: logs a warning and closes `conn` without serving anything.
        template <IsConnection ConnT>
        AsyncVoid serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http3Driver::serve() not implemented (scaffold)";
            beavers::force_non_const(this);
            co_await conn.async_close();
        }

    private:
        CROW_COMPONENT_PREFIX("Http3Driver");
    };

}  // namespace menagerie::http
