#pragma once

#include <cstddef>
#include <menagerie/beavers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <async_outcome.hpp>
#include <boost/asio/awaitable.hpp>
#include <errors.hpp>
#include <json/json.h>

namespace menagerie::http {

    /// One field of a parsed multipart/form-data body.
    struct MultipartField {
        std::string name;        ///< Form field name.
        std::string value;      ///< Payload bytes (small/non-file fields).
        std::string content_type;  ///< Declared MIME type of `value`.
        std::string filename;   ///< Empty for non-file fields.
    };

    namespace detail {
        /// Body::owned's payload. Lives in the header (unlike the other
        /// payloads, body.cpp-private) so the forwarding owned<S>() below can
        /// construct the string IN PLACE inside the SBO slot - the response
        /// body travels literal-to-payload with zero intermediate std::strings.
        struct OwnedBufferPayload {
            std::string bytes;      ///< The owned buffer.
            bool consumed = false;  ///< Whether read_chunk() has already yielded `bytes`.
            // Direct-init ctor, not aggregate init: Body::owned forwards
            // string_view/literals here, and std::string's ctor from
            // string_view is explicit - aggregate COPY-init would reject it.
            /// Constructs `bytes` in place from `str` (literal, string_view, or std::string).
            template <beavers::IsStringLike StringTp>
            explicit OwnedBufferPayload(StringTp&& str)
                : bytes(std::forward<StringTp>(str)) {
            }
            /// Yields the whole buffer once, then nullopt on every later call.
            boost::asio::awaitable<std::optional<std::span<const std::byte>>, Strand> read_chunk() {
                if (consumed || bytes.empty()) {
                    consumed = true;
                    co_return std::nullopt;
                }
                consumed = true;
                co_return std::span{reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
            }
            /// Exact byte count of the buffered payload.
            [[nodiscard]] std::optional<std::size_t> size_hint() const {
                return bytes.size();
            }
            /// The whole buffer, always available (never a streaming payload).
            [[nodiscard]] std::optional<std::string_view> buffered_view() const {
                return bytes;
            }
        };
    }  // namespace detail

    /**
     * @brief Streaming-truth body, held as an SBO value type.
     *
     * Type-erased over a payload (EmptyBody, OwnedBufferBody, BeastRequestBody
     * today; StreamingProducerBody planned). Common bodies live entirely
     * inline - zero heap nodes, no unique_ptr, no dynamic_cast. The driver
     * writes a body by driving read_chunk(), or, for non-streaming bodies, by
     * writing buffered_view() in one shot.
     */
    class Body : beavers::NonCopyable {
    public:
        Body() noexcept;                   ///< Constructs an EmptyBody.
        Body(Body&&) noexcept;             ///< Moves the active payload via the vtable.
        Body& operator=(Body&&) noexcept;  ///< Moves the active payload via the vtable.
        ~Body();                           ///< Destroys the active payload via the vtable.

        /// Returns an empty body (equivalent to the default constructor).
        static Body empty() noexcept {
            return Body{};
        }

        /// OwnedBufferBody. Forwarding: `bytes` (string literal, string_view,
        /// std::string lvalue/rvalue) initializes the payload's string directly
        /// in the SBO slot - no by-value relay moves. The only possible throw
        /// is an unrecoverable bad_alloc from that string's construction
        /// (UNRECOVERABLE_NOEXCEPT - terminate by default).
        template <beavers::IsStringLike StringTp = std::string>
        static Body owned(StringTp&& bytes) UNRECOVERABLE_NOEXCEPT {
            return Body{emplace_t{}, std::in_place_type<detail::OwnedBufferPayload>, std::forward<StringTp>(bytes)};
        }

        /// Non-owning view over bytes the caller keeps alive (the request arena
        /// owns the parsed h1 body). The span must outlive this Body (the
        /// driver keeps the parser/message alive across dispatch + write, then
        /// resets the arena).
        static Body beast_view(std::span<const std::byte> bytes);

        /// Streaming primitive: yields the next chunk, or nullopt when
        /// exhausted. A plain function returning the payload's own awaitable -
        /// no extra coroutine frame here.
        [[nodiscard]] boost::asio::awaitable<std::optional<std::span<const std::byte>>, Strand> read_chunk();

        /// Best-effort byte-length estimate; nullopt when the payload cannot
        /// know its size in advance (e.g. a live stream).
        [[nodiscard]] std::optional<std::size_t> size_hint() const;

        /// Whole-body view for non-streaming bodies (driver fast-path + tests).
        /// nullopt for streaming bodies; "" for EmptyBody.
        [[nodiscard]] std::optional<std::string_view> buffered_view() const;

        // -- Buffered helpers --

        /// Buffers the whole body into a string; fails with BodyLimitExceeded
        /// if the payload exceeds `limit` bytes.
        AsyncOutcome<std::string, BodyLimitExceeded> read_to_string(std::size_t limit);

        /// Buffers the whole body and parses it as JSON, subject to `limit` bytes.
        AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded> read_json(std::size_t limit);

        /// Buffers the whole body and parses it as
        /// application/x-www-form-urlencoded, subject to `limit` bytes.
        AsyncOutcome<std::unordered_map<std::string, std::string>, FormParseError, BodyLimitExceeded>
        read_form(std::size_t limit);

        /// Buffers the whole body and parses it as multipart/form-data using
        /// `boundary`, subject to `limit` bytes.
        AsyncOutcome<std::vector<MultipartField>, MultipartParseError, BodyLimitExceeded>
        read_multipart(std::size_t limit, std::string_view boundary);

    private:
        static constexpr std::size_t INLINE_SIZE = 48;

        struct VTable {
            boost::asio::awaitable<std::optional<std::span<const std::byte>>, Strand> (*read_chunk)(void*);
            std::optional<std::size_t> (*size_hint)(const void*);
            std::optional<std::string_view> (*buffered_view)(const void*);
            void (*move)(void* dst, void* src) noexcept;  // move-construct dst, destroy src
            void (*destroy)(void*) noexcept;
        };

        template <typename T>
        static const VTable* vtable_for() noexcept {
            static const VTable vt{
                +[](void* p) { return static_cast<T*>(p)->read_chunk(); },
                +[](const void* p) { return static_cast<const T*>(p)->size_hint(); },
                +[](const void* p) { return static_cast<const T*>(p)->buffered_view(); },
                +[](void* d, void* s) noexcept {
                    ::new (d) T(std::move(*static_cast<T*>(s)));
                    static_cast<T*>(s)->~T();
                },
                +[](void* p) noexcept { static_cast<T*>(p)->~T(); },
            };
            return &vt;
        }

        struct emplace_t {};
        template <typename T, typename... A>
        explicit Body(emplace_t, std::in_place_type_t<T>, A&&... a)
            : vt_{vtable_for<T>()} {
            static_assert(sizeof(T) <= INLINE_SIZE, "Body payload exceeds SBO budget");
            static_assert(alignof(T) <= alignof(std::max_align_t));
            ::new (storage_) T(std::forward<A>(a)...);
        }

        void* obj() noexcept {
            return storage_;
        }
        [[nodiscard]] const void* obj() const noexcept {
            return storage_;
        }

        alignas(std::max_align_t) std::byte storage_[INLINE_SIZE]{};  // TODO: C++26 attr [indeterminate] apply
        const VTable* vt_;
    };

}  // namespace menagerie::http
