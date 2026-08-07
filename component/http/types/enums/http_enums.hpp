#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <boost/beast/http/verb.hpp>

/// Protocol-agnostic HTTP vocabulary: Request, Response, Headers, Body,
/// RequestContext, the built-in error types, the AsyncOutcome/AsyncResponse
/// coroutine aliases, the wire-format enums, and the concrete Executor/Strand
/// aliases everything else is typed on. Depends on nothing else in the
/// component.
namespace menagerie::http {

    /// Application-layer protocol negotiated for a connection.
    enum class Protocol : std::uint8_t { http1, http2, http3 };

    /// HTTP request methods this component recognizes. `unknown` is the
    /// catch-all for any verb outside this set.
    enum class HttpMethod : std::uint8_t {
        unknown,
        get,
        post,
        put,
        patch,
        del,
        head,
        options,
    };

    /// Number of HttpMethod enumerators INCLUDING `unknown` (slot 0). Sizes
    /// per-method dispatch arrays in the routing layer. `unknown` can never be
    /// registered (RouteRegistry throws), so its slot stays empty and an
    /// unrecognized incoming verb falls out as a 404 or 405 response instead.
    inline constexpr std::size_t HTTP_METHOD_COUNT = 8;

    /// HTTP response status codes this component can emit.
    enum class HttpStatus : std::uint16_t {
        ok                     = 200,
        created                = 201,
        accepted               = 202,
        no_content             = 204,
        moved_permanently      = 301,
        found                  = 302,
        see_other              = 303,
        not_modified           = 304,
        temporary_redirect     = 307,
        permanent_redirect     = 308,
        bad_request            = 400,
        unauthorized           = 401,
        forbidden              = 403,
        not_found              = 404,
        method_not_allowed     = 405,
        conflict               = 409,
        gone                   = 410,
        payload_too_large      = 413,
        unsupported_media_type = 415,
        unprocessable_entity   = 422,
        too_many_requests      = 429,
        internal_server_error  = 500,
        not_implemented        = 501,
        bad_gateway            = 502,
        service_unavailable    = 503,
        gateway_timeout        = 504,
    };

    /// Wire protocol version, encoded as major*10 + minor to match Beast's
    /// numeric convention.
    enum class HttpVersion : std::uint8_t {
        http_1_0 = 10,
        http_1_1 = 11,
        http_2   = 20,
        http_3   = 30,
    };

    /// Renders a method as its uppercase wire-format token (e.g. "GET").
    /// HttpMethod::unknown, and any value outside the enum, renders as "UNKNOWN".
    constexpr std::string_view to_string_view(const HttpMethod m) noexcept {
        switch (m) {
            case HttpMethod::get:
                return "GET";
            case HttpMethod::post:
                return "POST";
            case HttpMethod::put:
                return "PUT";
            case HttpMethod::patch:
                return "PATCH";
            case HttpMethod::del:
                return "DELETE";
            case HttpMethod::head:
                return "HEAD";
            case HttpMethod::options:
                return "OPTIONS";
            case HttpMethod::unknown:
                return "UNKNOWN";
        }
        return "UNKNOWN";
    }

    /// Converts a Boost.Beast verb to the equivalent HttpMethod; any verb this
    /// component does not model becomes HttpMethod::unknown.
    constexpr HttpMethod method_from_beast(const boost::beast::http::verb v) noexcept {
        using V = boost::beast::http::verb;
        switch (v) {
            case V::get:
                return HttpMethod::get;
            case V::post:
                return HttpMethod::post;
            case V::put:
                return HttpMethod::put;
            case V::patch:
                return HttpMethod::patch;
            case V::delete_:
                return HttpMethod::del;
            case V::head:
                return HttpMethod::head;
            case V::options:
                return HttpMethod::options;
            default:
                return HttpMethod::unknown;
        }
    }

    /// Converts an HttpMethod back to the equivalent Boost.Beast verb;
    /// HttpMethod::unknown becomes verb::unknown.
    constexpr boost::beast::http::verb method_to_beast(const HttpMethod m) noexcept {
        using V = boost::beast::http::verb;
        switch (m) {
            case HttpMethod::get:
                return V::get;
            case HttpMethod::post:
                return V::post;
            case HttpMethod::put:
                return V::put;
            case HttpMethod::patch:
                return V::patch;
            case HttpMethod::del:
                return V::delete_;
            case HttpMethod::head:
                return V::head;
            case HttpMethod::options:
                return V::options;
            case HttpMethod::unknown:
                return V::unknown;
        }
        return V::unknown;
    }

    /// Converts Beast's numeric HTTP version (10, 11, 20, 30) to HttpVersion;
    /// anything unrecognized defaults to HTTP/1.1.
    constexpr HttpVersion version_from_beast(const unsigned v) noexcept {
        switch (v) {
            case 10:
                return HttpVersion::http_1_0;
            case 11:
                return HttpVersion::http_1_1;
            case 20:
                return HttpVersion::http_2;
            case 30:
                return HttpVersion::http_3;
            default:
                return HttpVersion::http_1_1;
        }
    }

    /// Converts an HttpVersion back to Beast's numeric encoding.
    constexpr unsigned version_to_beast(HttpVersion v) noexcept {
        return static_cast<unsigned>(v);
    }

}  // namespace menagerie::http
