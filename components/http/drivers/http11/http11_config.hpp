#pragma once

#include <chrono>
#include <cstddef>

namespace menagerie::http {

    /// Per-driver HTTP/1.1 limits + phase timeouts.
    /// attach_default_listeners derives one from ServerConfig (body_limit +
    /// phase timeouts); direct construction remains for per-driver tuning.
    struct Http11Config {
        std::size_t max_header_bytes = 16 * 1024;  ///< Rejects a request whose headers exceed this with 400.
        std::size_t max_body_bytes   = 16 * 1024 * 1024;  ///< Rejects a request whose body exceeds this with 413.

        std::chrono::milliseconds header_timeout = std::chrono::seconds{10};  ///< Bound on receiving a request's headers.
        std::chrono::milliseconds body_timeout   = std::chrono::seconds{30};  ///< Bound on receiving a request's body.
        /// Bounds the keep-alive wait for the NEXT request (empty input buffer
        /// at message start). header_timeout takes over once a message is
        /// mid-arrival; body_timeout once its header completes.
        std::chrono::milliseconds idle_timeout   = std::chrono::seconds{60};
    };

}  // namespace menagerie::http
