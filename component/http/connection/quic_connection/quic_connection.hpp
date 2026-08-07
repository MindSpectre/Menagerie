#pragma once

#include <chrono>
#include <cstddef>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <executor.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace menagerie::http {

    /// QUIC connection - SCAFFOLD. Unimplemented placeholder: it satisfies
    /// IsConnection (arena + lifecycle) so it compiles and links against the
    /// rest of the stack today, but carries no real QUIC transport state yet;
    /// ngtcp2 state lands when h3 is implemented. Deliberately NOT
    /// IsStreamConnection (QUIC is not a single byte stream).
    class QuicConnection : beavers::Immutable {
    public:
        /// Allocates the request arena; no transport-specific setup yet.
        explicit QuicConnection(const std::size_t arena_size = 8192)
            : arena_{arena_size} {
        }

        /// Allocator bound to this connection's per-request arena (IsConnection).
        [[nodiscard]] std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        /// Rewinds the arena between requests (IsConnection).
        void reset_request_arena() {
            arena_.reset();
        }

        /// Scaffold no-op (IsConnection); QUIC timeout enforcement arrives
        /// with the real h3 transport. See TcpConnection::set_deadline_after.
        void set_deadline_after(std::chrono::milliseconds) noexcept {
            beavers::force_non_const(this);
        }

        /// Scaffold no-op (IsConnection); nothing to close until the QUIC
        /// transport lands.
        boost::asio::awaitable<void, Strand> async_close() {
            beavers::force_non_const(this);
            co_return;
        }

        /// Cancellation slot for this connection's signal (IsConnection).
        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }

        /// Scaffold stub (IsConnection); returns a default-constructed
        /// address until the QUIC transport parses the peer's address.
        [[nodiscard]] boost::asio::ip::address remote_address() const {
            beavers::force_non_static(this);
            return {};
        }

        /// Always http3 (IsConnection) - the only protocol QuicConnection
        /// will ever serve.
        [[nodiscard]] static Protocol negotiated_protocol() noexcept {
            return Protocol::http3;
        }

        /// Always true (IsConnection) - QUIC is TLS 1.3 by construction.
        [[nodiscard]] static bool is_secure() noexcept {
            return true;
        }

    private:
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

}  // namespace menagerie::http
