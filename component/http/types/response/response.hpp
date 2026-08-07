#pragma once

#include <memory_resource>
#include <menagerie/beavers>
#include <string_view>
#include <utility>

#include <body.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
namespace menagerie::http {

    /**
     * @brief Protocol-agnostic HTTP response.
     *
     * STORES its allocator: built on the hot path through RequestContext, it
     * points at the request arena, so even middleware that mutates the
     * response AFTER the handler returns keeps allocations in the arena.
     * Default-constructed (ctx-less / error / test) it uses new_delete - the
     * cold path. Body is a value type. Drivers stamp Date/Server.
     */
    struct Response : beavers::NonCopyable {
        std::pmr::polymorphic_allocator<> alloc{};  ///< Sticky; never reassigned by move-assign.
        HttpStatus status   = HttpStatus::ok;        ///< Response status code.
        HttpVersion version = HttpVersion::http_1_1;  ///< Wire protocol version.
        bool keep_alive     = true;                   ///< Whether the connection stays open after this response.
        Headers headers;  ///< Response headers.
        Body body;  ///< Default EmptyBody.

        /// Constructs an empty 200 OK response bound to `a` (the request arena
        /// on the hot path; the global heap by default).
        explicit Response(const std::pmr::polymorphic_allocator<> a = {})
            : alloc{a},
              headers{Headers::owned(a)} {
        }

        /// Move-constructible.
        Response(Response&&) = default;

        /**
         * @brief Move-assigns, keeping `alloc` sticky.
         *
         * pmr::polymorphic_allocator has a deleted copy-assign, so the
         * defaulted move-assign is also deleted. This override moves the
         * payload fields but leaves `alloc` untouched, since the resource it
         * refers to is owned by the arena that outlives this struct.
         */
        Response& operator=(Response&& other) noexcept {
            if (this != &other) {
                status     = other.status;
                version    = other.version;
                keep_alive = other.keep_alive;
                headers    = std::move(other.headers);
                body       = std::move(other.body);
                // alloc is intentionally NOT reassigned - it is sticky.
            }
            return *this;
        }

        // -- Fluent setters (deducing this; chain on lvalues + rvalues) ---------

        /// Sets the status code; chainable.
        template <typename Self>
        constexpr auto&& with_status(this Self&& self, const HttpStatus s) noexcept {
            self.status = s;
            return std::forward<Self>(self);
        }
        /// Sets the wire protocol version; chainable.
        template <typename Self>
        constexpr auto&& with_version(this Self&& self, const HttpVersion v) noexcept {
            self.version = v;
            return std::forward<Self>(self);
        }
        /// Sets whether the connection stays open after this response; chainable.
        template <typename Self>
        constexpr auto&& with_keep_alive(this Self&& self, const bool k) noexcept {
            self.keep_alive = k;
            return std::forward<Self>(self);
        }
        /// Replaces all values of header `n` with `v`; chainable.
        template <typename Self>
        constexpr auto&& set_header(this Self&& self, const std::string_view n, const std::string_view v) {
            self.headers.set(n, v);
            return std::forward<Self>(self);
        }
        /// Appends header `n: v`, keeping any existing value(s); chainable.
        template <typename Self>
        constexpr auto&& add_header(this Self&& self, const std::string_view n, const std::string_view v) {
            self.headers.add(n, v);
            return std::forward<Self>(self);
        }
        /// Sets the response body to `content`; chainable.
        template <typename Self, beavers::IsStringLike StringTp>
        constexpr auto&& with_body(this Self&& self, StringTp&& content) {
            self.body = Body::owned(std::forward<StringTp>(content));
            return std::forward<Self>(self);
        }
    };

}  // namespace menagerie::http
