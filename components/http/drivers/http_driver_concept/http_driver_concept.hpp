#pragma once

#include <concepts>
#include <span>
#include <string_view>

#include <http_enums.hpp>

/// The protocol drivers under drivers/: each implements one wire format's
/// serve(connection, Router&) coroutine over a connection satisfying
/// IsConnection/IsStreamConnection from connection/.
namespace menagerie::http {

    /**
     * @brief What every protocol driver advertises statically.
     *
     * serve(conn, router) is intentionally NOT in the concept: it is
     * templated on the connection type and checked where the listener pairs a
     * driver with a connection. The build/buy line is inside serve().
     */
    template <typename T>
    concept IsHttpDriver = requires {
        { T::id() } -> std::same_as<Protocol>;
        { T::accepted_alpns() } -> std::same_as<std::span<const std::string_view>>;
    };

}  // namespace menagerie::http
