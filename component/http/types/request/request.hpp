#pragma once

#include <menagerie/beavers>
#include <string_view>

#include <body.hpp>
#include <headers.hpp>
#include <http_enums.hpp>

namespace menagerie::http {

    /**
     * @brief Protocol-agnostic HTTP request.
     *
     * `target` is the raw, undecoded request target as a VIEW into the receive
     * buffer (e.g. "/users/42?q=foo"). It, like the headers and body, is valid
     * only while the owning connection's buffers live, i.e. for the duration of
     * the handler. Copy out anything you keep. Path/query split + URL decode
     * happen in RequestContext.
     *
     * `headers` and `body` are move-only value types. A Request must be
     * constructed with a Headers bound to an allocator (there is no null state).
     */
    struct Request : beavers::NonCopyable {
        HttpMethod method   = HttpMethod::unknown;  ///< Parsed request method.
        HttpVersion version = HttpVersion::http_1_1;  ///< Wire protocol version.
        std::string_view target;  ///< Raw request target; a view into the receive buffer.
        Headers headers;          ///< Move-only; bound to an allocator.
        Body body;                ///< Value type; default EmptyBody.

        /// Constructs a Request over an already-allocator-bound Headers.
        explicit Request(Headers hdrs)
            : headers{std::move(hdrs)} {
        }
    };

}  // namespace menagerie::http
