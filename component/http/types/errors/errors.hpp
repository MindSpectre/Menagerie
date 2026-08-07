#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <http_enums.hpp>

namespace menagerie::http {

    struct Response;  // forward declaration; defined in response/response.hpp

    /// Maps to a 400 response.
    struct BadRequestError {
        std::string message;  ///< Human-readable failure description.
    };

    /// Maps to a 401 response.
    struct UnauthorizedError {
        std::string message;  ///< Human-readable failure description.
    };

    /// Maps to a 403 response.
    struct ForbiddenError {
        std::string message;  ///< Human-readable failure description.
    };

    /// Maps to a 404 response.
    struct NotFoundError {
        std::string resource;  ///< Kind of resource that was not found (e.g. "user").
        std::string id;        ///< Identifier that was looked up, if any.
    };

    /// Maps to a 409 response.
    struct ConflictError {
        std::string message;  ///< Human-readable failure description.
    };

    /// One field-level validation failure, used by UnprocessableEntityError.
    struct FieldError {
        std::string field;   ///< Name of the offending field.
        std::string detail;  ///< Human-readable description of the failure.
    };

    /// Maps to a 422 response; `fields` lists the individual field failures.
    struct UnprocessableEntityError {
        std::string message;             ///< Overall human-readable failure description.
        std::vector<FieldError> fields;  ///< Per-field validation failures.
    };

    /// Maps to a 413 response.
    struct PayloadTooLargeError {
        std::size_t limit = 0;  ///< The configured body-size limit that was exceeded, in bytes.
    };

    /// Maps to a 405 response, with `Allow:` populated from `allowed`.
    struct MethodNotAllowedError {
        std::vector<HttpMethod> allowed;  ///< Methods the matched route accepts.
    };

    /// Maps to a 400 response.
    struct JsonParseError {
        std::string detail;  ///< Parser's human-readable failure description.
    };

    /// Maps to a 400 response.
    struct FormParseError {
        std::string detail;  ///< Parser's human-readable failure description.
    };

    /// Maps to a 400 response.
    struct MultipartParseError {
        std::string detail;  ///< Parser's human-readable failure description.
    };

    /// Maps to a 413 response.
    struct BodyLimitExceeded {
        std::size_t limit = 0;  ///< The configured body-size limit that was exceeded, in bytes.
    };

    // -- ADL conversions --
    // Intentionally arena-free: error responses (4xx/5xx) are the cold path
    // and construct on the global heap via the static ResponseFactory. Keeps
    // the user's extension point a clean 1-arg free function.

    /// Converts each built-in error type to its mapped Response, found by
    /// argument-dependent lookup wherever an AsyncOutcome error alternative
    /// needs collapsing to a Response.
    Response to_http_response(const BadRequestError& e);
    /// Converts to a 401 response.
    Response to_http_response(const UnauthorizedError& e);
    /// Converts to a 403 response.
    Response to_http_response(const ForbiddenError& e);
    /// Converts to a 404 response.
    Response to_http_response(const NotFoundError& e);
    /// Converts to a 409 response.
    Response to_http_response(const ConflictError& e);
    /// Converts to a 422 response listing `e.fields`.
    Response to_http_response(const UnprocessableEntityError& e);
    /// Converts to a 413 response.
    Response to_http_response(const PayloadTooLargeError& e);
    /// Converts to a 405 response with `Allow:` populated from `e.allowed`.
    Response to_http_response(const MethodNotAllowedError& e);
    /// Converts to a 400 response.
    Response to_http_response(const JsonParseError& e);
    /// Converts to a 400 response.
    Response to_http_response(const FormParseError& e);
    /// Converts to a 400 response.
    Response to_http_response(const MultipartParseError& e);
    /// Converts to a 413 response.
    Response to_http_response(const BodyLimitExceeded& e);

}  // namespace menagerie::http
