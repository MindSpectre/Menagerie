#pragma once

#include <charconv>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>

#include <body.hpp>
#include <boost/container/small_vector.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
#include <request.hpp>
#include <response.hpp>

namespace menagerie::http {

    /**
     * @brief Handler-facing view of one in-flight request.
     *
     * Owns the moved-in Request + the request arena allocator. Header lookup is
     * lazy. The target-to-(path, query) split is memoized; since `target` is a
     * VIEW into stable connection-owned storage, the cached views survive moves
     * (the old owned-string SSO dangle is gone). Move-only; passed by value
     * into handlers. Valid only for the handler's duration.
     */
    class RequestContext {
    public:
        /// Takes ownership of `req`; `alloc` is the request arena that lazily
        /// built views, parameters, and the response factories below allocate
        /// from.
        RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc);

        /// Move-constructible; passed by value down the middleware chain.
        RequestContext(RequestContext&&)                 = default;
        // Move-ASSIGN is deleted: a defaulted one would replace bag_ without
        // running the old payloads' destructors (a real leak for owning
        // payloads). Contexts are move-CONSTRUCTED through the chain; nothing
        // ever assigns over one.
        RequestContext& operator=(RequestContext&&)      = delete;
        RequestContext(const RequestContext&)            = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        /// The request's HTTP method.
        HttpMethod method() const noexcept {
            return request_.method;
        }
        /// The request's wire protocol version.
        HttpVersion version() const noexcept {
            return request_.version;
        }
        /// The raw, undecoded request target (path + query string).
        std::string_view target() const noexcept {
            return request_.target;
        }
        /// The undecoded path portion of `target`, split off from the query string.
        std::string_view path() const;
        /// The raw query string portion of `target`, without the leading '?'.
        std::string_view query_string() const;
        /// The request's headers.
        const Headers& headers() const noexcept {
            return request_.headers;
        }
        /// The request's body.
        Body& body() noexcept {
            return request_.body;
        }

        /// The first value of the header named `name`, if present.
        std::optional<std::string_view> header(const std::string_view name) const {
            return request_.headers.get(name);
        }
        /// Like header(), but returns `fallback` instead of nullopt when absent.
        std::string header_or(const std::string_view name, const std::string_view fallback) const {
            return request_.headers.get_or(name, fallback);
        }

        /// Whether Content-Type indicates a JSON body.
        bool is_json() const;
        /// Whether Content-Type indicates an application/x-www-form-urlencoded body.
        bool is_form() const;
        /// Whether Content-Type indicates a multipart/form-data body.
        bool is_multipart() const;
        /// Whether Accept indicates the client will take a JSON response.
        bool accepts_json() const;
        /// Whether Accept indicates the client will take an HTML response.
        bool accepts_html() const;

        /// The request arena allocator this context and its responses are bound to.
        std::pmr::polymorphic_allocator<> arena_alloc() const noexcept {
            return alloc_;
        }

        // -- Path parameters (set by the routing layer) --
        /// Records a matched path-parameter capture; called by the router
        /// after a successful route match, before the handler runs.
        void set_path_param(std::string_view name, std::string_view value);

        /// The path parameter named `name`, converted to T; nullopt if absent
        /// or if the raw value fails to convert.
        template <typename T>
        std::optional<T> path_param(const std::string_view name) const {
            auto raw = raw_path_param(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        /// Like path_param(), but returns `fallback` instead of nullopt.
        template <typename T>
        T path_param_or(const std::string_view name, T fallback) const {
            if (auto v = path_param<T>(name))
                return *std::move(v);
            return fallback;
        }

        // -- Query parameters (lazily parsed from query_string) --
        /// The query parameter named `name`, converted to T; nullopt if absent
        /// or if the raw value fails to convert. Triggers query-string parsing
        /// on first use.
        template <typename T>
        std::optional<T> query(const std::string_view name) const {
            auto raw = raw_query(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        /// Like query(), but returns `fallback` instead of nullopt.
        template <typename T>
        T query_or(const std::string_view name, T fallback) const {
            if (auto v = query<T>(name))
                return *std::move(v);
            return fallback;
        }

        // -- Type-keyed middleware bag (arena-backed) --
        // Forwarding sink: the payload is constructed in its arena slot
        // directly from the argument - no by-value relay copy+move. The bag
        // key is the DECAYED type, so set(lvalue)/set(rvalue)/set<T>(...) all
        // address the same entry.
        /// Stores `value`, keyed by its decayed type, for later retrieval via
        /// `get<T>()`/`has<T>()`; a second set() of the same type replaces the
        /// stored value. Used by middleware to enrich the context for
        /// downstream handlers (e.g. an authenticated-user payload).
        template <typename T>
        void set(T&& value) {
            using D = std::remove_cvref_t<T>;
            static_assert(std::is_constructible_v<D, T&&>);
            const std::type_index key{typeid(D)};
            if (auto* e = find_bag_entry(key)) {
                // Construct the new payload BEFORE destroying the old one, so a
                // throwing construction leaves the existing payload intact
                // (strong guarantee) rather than a live destroyer over
                // destroyed bytes.
                void* mem = alloc_.allocate_bytes(sizeof(D), alignof(D));
                ::new (mem) D(std::forward<T>(value));
                e->destroyer(e->ptr);
                e->ptr       = mem;
                e->destroyer = +[](void* p) noexcept { static_cast<D*>(p)->~D(); };
                return;
            }
            void* mem = alloc_.allocate_bytes(sizeof(D), alignof(D));
            ::new (mem) D(std::forward<T>(value));
            bag_.push_back(BagEntry{key, mem, +[](void* p) noexcept { static_cast<D*>(p)->~D(); }});
        }
        /// The bag entry of type T, or nullptr if set<T>() was never called.
        template <typename T>
        T* get() {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<T*>(e->ptr) : nullptr;
        }
        /// The bag entry of type T, or nullptr if set<T>() was never called.
        template <typename T>
        const T* get() const {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<const T*>(e->ptr) : nullptr;
        }
        /// Whether a bag entry of type T is present.
        template <typename T>
        bool has() const {
            return find_bag_entry(std::type_index{typeid(T)}) != nullptr;
        }

        // -- Arena-bound response factories (hot path) --
        // Forwarding: `body` (string literal, string_view, std::string
        // lvalue/rvalue) travels as-is into Body's SBO payload, where the
        // std::string is constructed ONCE, in place. Only possible throw is
        // an unrecoverable bad_alloc (UNRECOVERABLE_NOEXCEPT - terminate by
        // default).

        /// 200 OK with `body` (text/plain by default), allocated in the request arena.
        template <beavers::IsStringViewLike StringTp = std::string_view>
        Response ok(StringTp&& body = {}, const std::string_view ct = "text/plain") UNRECOVERABLE_NOEXCEPT {
            return make_response(HttpStatus::ok, ct, true, std::forward<StringTp>(body));
        }
        /// 200 OK with `body` as application/json, allocated in the request arena.
        template <beavers::IsStringViewLike StringTp>
        Response json(StringTp&& body) UNRECOVERABLE_NOEXCEPT {
            return make_response(HttpStatus::ok, "application/json", true, std::forward<StringTp>(body));
        }
        /// 201 Created with `body` (application/json by default), allocated in the request arena.
        template <beavers::IsStringViewLike StringTp = std::string_view>
        Response created(StringTp&& body = {}, const std::string_view ct = "application/json") UNRECOVERABLE_NOEXCEPT {
            return make_response(HttpStatus::created, ct, true, std::forward<StringTp>(body));
        }
        /// 204 No Content, allocated in the request arena.
        Response no_content() const;
        /// A redirect response with a `Location: location` header, allocated
        /// in the request arena; `status` defaults to 302 Found.
        Response redirect(std::string_view location, HttpStatus status = HttpStatus::found) const;
        /// An arbitrary `s` status response with `body`, allocated in the request arena.
        template <beavers::IsStringViewLike StringTp = std::string_view>
        Response status(const HttpStatus s,
                        StringTp&& body           = {},
                        const std::string_view ct = "text/plain") UNRECOVERABLE_NOEXCEPT {
            return make_response(s, ct, true, std::forward<StringTp>(body));
        }

        /// Runs the destroyers of any live type-keyed bag entries this
        /// context still owns (a moved-from context's bag holds none).
        ~RequestContext();

    private:
        // IsStringViewLike, not just IsStringLike: the body must be viewable
        // without consuming for the empty check below.
        template <beavers::IsStringViewLike StringTp>
        Response make_response(const HttpStatus s, const std::string_view ct, const bool with_ct, StringTp&& body) {
            Response r{alloc_};  // alloc + headers bound to the arena
            r.status = s;
            if (with_ct)
                r.add_header("Content-Type", ct);
            if (!std::string_view{body}.empty()) [[likely]]
                r.body = Body::owned(std::forward<StringTp>(body));
            return r;
        }

        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        using ParamEntry = std::pair<std::pmr::string, std::pmr::string>;
        // Inline capacity 1, not 4: ParamEntry is 64 B, so 4-slot inline
        // storage on TWO vectors plus the bag put ~600 B inside every
        // RequestContext - paid on each by-value move through the handler
        // chain and in every handler coroutine frame. Exact-match
        // routes carry ZERO entries; overflow beyond 1 lands in the request
        // arena (bump alloc), so multi-param routes pay one cheap arena grow.
        using ParamVec   = boost::container::small_vector<ParamEntry, 1, std::pmr::polymorphic_allocator<ParamEntry>>;

        ParamVec path_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};
        mutable bool query_parsed_ = false;
        mutable ParamVec query_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

        struct BagEntry {
            std::type_index key;
            void* ptr;
            void (*destroyer)(void*) noexcept;

            BagEntry(const std::type_index k, void* p, void (*d)(void*) noexcept) noexcept
                : key{k},
                  ptr{p},
                  destroyer{d} {
            }
            // Move nulls the source ptr, so a moved-from RequestContext's bag
            // runs NO destroyers - double-destruction is impossible regardless
            // of small_vector's moved-from element behaviour. (~RequestContext
            // guards on `ptr`.) RequestContext is moved by value through the
            // middleware chain carrying a populated bag, so this path is hot.
            BagEntry(BagEntry&& o) noexcept
                : key{o.key},
                  ptr{o.ptr},
                  destroyer{o.destroyer} {
                o.ptr = nullptr;
            }
            BagEntry& operator=(BagEntry&& o) noexcept {
                key       = o.key;
                ptr       = o.ptr;
                destroyer = o.destroyer;
                o.ptr     = nullptr;
                return *this;
            }
            BagEntry(const BagEntry&)            = delete;
            BagEntry& operator=(const BagEntry&) = delete;
        };
        boost::container::small_vector<BagEntry, 1, std::pmr::polymorphic_allocator<BagEntry>> bag_{
            std::pmr::polymorphic_allocator<BagEntry>{alloc_}};

        BagEntry* find_bag_entry(const std::type_index key) {
            for (auto& e : bag_)
                if (e.key == key)
                    return &e;
            return nullptr;
        }
        const BagEntry* find_bag_entry(const std::type_index key) const {
            for (const auto& e : bag_)
                if (e.key == key)
                    return &e;
            return nullptr;
        }

        mutable std::optional<std::string_view> cached_path_;
        mutable std::optional<std::string_view> cached_query_;
        void ensure_split() const;

        void ensure_query_parsed() const;
        std::optional<std::string_view> raw_query(std::string_view name) const;
        std::optional<std::string_view> raw_path_param(std::string_view name) const;

        // Defined in the header so consumers instantiate for their own T -
        // no .cpp explicit-instantiation list, no link cap on the type set.
        template <typename T>
        static std::optional<T> convert_string(std::string_view value) {
            if constexpr (std::is_same_v<T, std::string>) {
                return std::string{value};
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                return value;
            } else if constexpr (std::is_same_v<T, bool>) {
                // is_arithmetic_v<bool> is true but from_chars has no bool
                // overload - this branch must precede the arithmetic one.
                // Strict by design: "1"/"true" and "0"/"false" only.
                if (value == "1" || value == "true")
                    return true;
                if (value == "0" || value == "false")
                    return false;
                return std::nullopt;
            } else if constexpr (std::is_arithmetic_v<T>) {
                T out{};
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
                if (ec != std::errc{} || ptr != value.data() + value.size())
                    return std::nullopt;
                return out;
            } else {
                static_assert(sizeof(T) == 0, "RequestContext: unsupported param type");
                std::unreachable();
            }
        }
    };

}  // namespace menagerie::http
