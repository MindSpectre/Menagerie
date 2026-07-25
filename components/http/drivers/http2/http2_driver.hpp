#pragma once

#include <menagerie/crow>
#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace menagerie::http {

    /// HTTP/2 driver - SCAFFOLD. Unimplemented placeholder: it satisfies
    /// IsHttpDriver so the TLS listener can already select it via ALPN, but
    /// serve() just logs and closes the connection; the real nghttp2-backed
    /// implementation lands in a future change.
    class Http2Driver {
    public:
        /// Protocol tag this driver serves (IsHttpDriver).
        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http2;
        }

        /// ALPN protocol IDs this driver accepts (IsHttpDriver).
        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"h2"};
            return ALPNS;
        }

        /// Scaffold: logs a warning and closes `conn` without serving anything.
        template <IsStreamConnection ConnT>
        AsyncVoid serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http2Driver::serve() not implemented (scaffold)";
            beavers::force_non_const(this);
            co_await conn.async_close();
        }

    private:
        SCROLL_COMPONENT_PREFIX("Http2Driver");
    };

}  // namespace menagerie::http
