#pragma once

#include <span>
#include <string>
#include <string_view>

#include <http_enums.hpp>
#include <response.hpp>

namespace menagerie::http {

    /// Static, GLOBAL-HEAP, cold-path factory: error conversions
    /// (to_http_response) and library/test/synthetic responses. The hot path is
    /// RequestContext's arena-bound factories (ctx.json/ok/...). Neither sets
    /// Date/Server (drivers stamp those).
    class ResponseFactory {
    public:
        /// 200 OK with `body` and content type `ct`.
        static Response ok(std::string body = "", std::string_view ct = "text/plain");
        /// 200 OK with `body` as `application/json`.
        static Response json(std::string body);
        /// 201 Created with `body` and content type `ct`.
        static Response created(std::string body = "", std::string_view ct = "application/json");
        /// 204 No Content.
        static Response no_content();
        /// A redirect response with a `Location: location` header; `status`
        /// defaults to 302 Found.
        static Response redirect(std::string_view location, HttpStatus status = HttpStatus::found);
        /// 404 Not Found.
        static Response not_found(std::string body = "Not Found");
        /// 400 Bad Request.
        static Response bad_request(std::string body = "Bad Request");
        /// 401 Unauthorized.
        static Response unauthorized(std::string body = "Unauthorized");
        /// 403 Forbidden.
        static Response forbidden(std::string body = "Forbidden");
        /// 409 Conflict.
        static Response conflict(std::string body = "Conflict");
        /// 413 Payload Too Large.
        static Response payload_too_large(std::string body = "Payload Too Large");
        /// 422 Unprocessable Entity.
        static Response unprocessable_entity(std::string body = "Unprocessable Entity");
        /// 500 Internal Server Error.
        static Response internal_error(std::string body = "Internal Server Error");
        /// 405 Method Not Allowed, with the `Allow:` header populated from `allow`.
        static Response method_not_allowed(std::span<const HttpMethod> allow);
        /// An arbitrary `status` response with `body` and content type `ct`.
        static Response custom(HttpStatus status, std::string body, std::string_view ct = "text/plain");
    };

}  // namespace menagerie::http
