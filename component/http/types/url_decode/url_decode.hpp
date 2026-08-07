#pragma once

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>

namespace menagerie::http {

    /// RFC 3986 percent-decoding. With plus_is_space, '+' -> ' '
    /// (application/x-www-form-urlencoded - query strings and form bodies).
    /// PATH SEGMENTS must pass plus_is_space=false: '+' is a literal in paths.
    /// Returns nullopt on a malformed escape (truncated or non-hex) so callers
    /// can surface a typed error.
    std::optional<std::string> url_decode(std::string_view in, bool plus_is_space = true);

    /// Same decoding, allocating from `alloc` (the request arena) instead of
    /// the global heap, since this runs on the routing hot path. Returns the
    /// INPUT view unchanged when no rewrite is needed (no '%', and no '+' in
    /// plus_is_space mode) - the common zero-copy case. Otherwise the result
    /// views arena storage: valid until the arena resets, never individually
    /// freed (monotonic). nullopt on a malformed escape.
    std::optional<std::string_view>
    url_decode_arena(std::string_view in, bool plus_is_space, std::pmr::polymorphic_allocator<> alloc);

}  // namespace menagerie::http
