#pragma once

#include <expected>
#include <variant>

#include <boost/asio/awaitable.hpp>
#include <executor.hpp>

namespace menagerie::albatross {

    struct Response;  // forward declaration; defined in response/response.hpp

    /// asio coroutine yielding a typed-error sum result,
    /// `std::expected<T, std::variant<Es...>>`. Handlers/middleware return
    /// AsyncOutcome<Response, Errors...>; the bind layer collapses a held error
    /// into a plain Response via ADL to_http_response.
    ///
    /// The error set is ALWAYS a std::variant, even for a single Es, so every
    /// AsyncOutcome has one shape: `if (!r)` tests failure, `r.error()` is the
    /// variant, and a handler whose error set matches a callee's forwards with
    /// `co_return std::unexpected(std::move(r).error());`. `std::unexpected(e)`
    /// converts into any AsyncOutcome whose error set contains E's type.
    /// Inspect a held error with std::holds_alternative<E>(r.error()) /
    /// std::get<E>(r.error()). Es... must not be empty (use AsyncResponse).
    ///
    /// Typed on the connection Strand, NOT the default any_io_executor:
    /// handlers always run on the connection's strand, and an erased frame
    /// re-introduces virtual executor dispatch on every await in the request
    /// loop. Ops co_awaited inside these coroutines must use the matching
    /// `use_strand_awaitable` token (executor.hpp).
    template <typename T, typename... Es>
    using AsyncOutcome = boost::asio::awaitable<std::expected<T, std::variant<Es...>>, Strand>;

    /// The common "no typed errors" case. Response is a struct - the forward
    /// declaration above suffices for the alias; users who instantiate it
    /// include <response/response.hpp>.
    using AsyncResponse = boost::asio::awaitable<Response, Strand>;

    /// An asio coroutine yielding nothing, typed on the connection Strand.
    using AsyncVoid = boost::asio::awaitable<void, Strand>;

}  // namespace menagerie::albatross
