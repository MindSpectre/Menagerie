#pragma once

#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <menagerie/beavers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/beast/http/fields.hpp>

namespace menagerie::http {

    /**
     * @brief The Beast fields type an incoming request is parsed into.
     *
     * pmr, not `std::allocator`: Beast allocates one node per header line plus
     * one for the request target, and with the default allocator every one of
     * those is a global-heap malloc/free per request (~4 measured). Binding it
     * to the per-connection request arena makes them bump allocations that the
     * arena reset reclaims for free.
     *
     * This is a single concrete type on purpose. Templating `Headers` on the
     * fields allocator would push the parameter through BeastBacking, the
     * iterator, and every consumer, for no benefit - nothing constructs Headers
     * over a differently-allocated fields object.
     */
    using BeastFields = boost::beast::http::basic_fields<std::pmr::polymorphic_allocator<char>>;

    /**
     * @brief Multi-value, case-insensitive, insertion-ordered HTTP headers.
     *
     * Two backings behind one API:
     *   BeastBacking - read-only view over a parser-owned BeastFields
     *                  (incoming requests, zero copy).
     *   OwnedBacking - arena-owned pmr::string pairs (responses, h2/h3 incoming,
     *                  synthetic/test). ALWAYS carries its allocator.
     *
     * There is no default/null state: construct via owned(alloc) or
     * view_of_beast(fields). Mutators and promotion allocate through the bound
     * allocator - never the global heap.
     */
    class Headers : beavers::NonCopyable {
    public:
        /// A single header entry: (name, value) as string_views into the
        /// backing storage.
        using value_type = std::pair<std::string_view, std::string_view>;

        // -- Factories --
        /// Allocates the entry vector's first block through `alloc`; the only
        /// possible throw is an unrecoverable bad_alloc
        /// (UNRECOVERABLE_NOEXCEPT - terminate by default).
        static Headers owned(std::pmr::polymorphic_allocator<> alloc) UNRECOVERABLE_NOEXCEPT;
        /// Non-owning view. `fields` MUST outlive the returned Headers.
        static Headers view_of_beast(const BeastFields& fields);

        /// Beast's basic_fields has an implicit converting ctor from a
        /// differently-allocated basic_fields. Without these deletions,
        /// `view_of_beast(some_http_fields)` would materialize a temporary
        /// BeastFields, take its address, and dangle the moment the full
        /// expression ends - a segfault, not a compile error. Ask for it and get
        /// a diagnostic instead.
        template <class OtherAlloc>
        static Headers view_of_beast(const boost::beast::http::basic_fields<OtherAlloc>&) = delete;
        static Headers view_of_beast(BeastFields&&)                                       = delete;

        /// Move-only: Headers may hold a pmr container.
        Headers(Headers&&) = default;

        /**
         * @brief Move-assigns, adopting the source's backing wholesale.
         *
         * User-defined rather than defaulted: variant emplace triggers a
         * vector move-construct that steals the source's buffer and
         * allocator. A defaulted move-assign hits the pmr POCMA=false trap -
         * element-wise COPY into the destination's OLD allocator, so
         * `Response r; r = co_await next(ctx);` would silently copy arena
         * headers onto the global heap.
         */
        Headers& operator=(Headers&& other) noexcept;

        // -- Read API --
        /// The first value of the header named `name` (case-insensitive), if present.
        [[nodiscard]] std::optional<std::string_view> get(std::string_view name) const;
        /// Like get(), but returns `fallback` instead of nullopt when absent.
        [[nodiscard]] std::string get_or(std::string_view name, std::string_view fallback) const;
        /// Every value of the header named `name` (case-insensitive; e.g. a
        /// repeated Set-Cookie), allocated in `alloc`.
        [[nodiscard]] std::pmr::vector<std::string_view> get_all(std::string_view name,
                                                                 std::pmr::polymorphic_allocator<> alloc) const;
        /// Whether a header named `name` is present.
        [[nodiscard]] bool contains(std::string_view name) const;
        /// Number of header entries (repeated headers count once per value).
        [[nodiscard]] std::size_t size() const;
        /// Whether there are no header entries.
        [[nodiscard]] bool empty() const {
            return size() == 0;
        }

        // -- Write API (requires OwnedBacking; promote first if viewing) ------
        /// Appends a new entry, keeping any existing value(s) for `name`.
        void add(std::string_view name, std::string_view value);
        /// Replaces all existing values for `name` with a single `value`, or
        /// adds it if `name` is absent.
        void set(std::string_view name, std::string_view value);
        /// Removes every entry named `name`.
        void remove(std::string_view name);

        /// Copy a BeastBacking into a fresh OwnedBacking in `alloc`. No-op if
        /// already owned. MUST be called before mutating a viewing Headers.
        void promote_to_owned(std::pmr::polymorphic_allocator<> alloc);

        // -- Iteration (O(1) per step) --
        /// Forward-only, O(1)-per-step iterator over (name, value) pairs;
        /// dereferencing returns the pair by value, so it satisfies
        /// input_iterator, not forward_iterator.
        class const_iterator {
        public:
            using value_type        = value_type;  ///< Same as Headers::value_type.
            using reference         = value_type;  ///< Dereferencing yields a value, not a real reference.
            using difference_type   = std::ptrdiff_t;  ///< Standard iterator difference type.
            // input, not forward: operator* returns a proxy pair by value.
            using iterator_category = std::input_iterator_tag;  ///< Single-pass only (proxy dereference).

            const_iterator() = default;
            /// Returns the (name, value) pair at the current position.
            value_type operator*() const;
            /// Advances to the next (name, value) pair.
            const_iterator& operator++();
            /// Postfix advance; returns the pre-increment position.
            const_iterator operator++(int);
            /// Whether `a` and `b` refer to the same position in the same Headers.
            friend bool operator==(const const_iterator& a, const const_iterator& b) {
                return a.idx_ == b.idx_ && a.h_ == b.h_;
            }

        private:
            friend class Headers;
            const Headers* h_ = nullptr;
            std::size_t idx_  = 0;  // position; also drives beast_it_ advance
            BeastFields::const_iterator beast_it_{};
        };

        /// Iterator to the first (name, value) pair, in insertion order.
        [[nodiscard]] const_iterator begin() const;
        /// Iterator past the last (name, value) pair.
        [[nodiscard]] const_iterator end() const;

    private:
        struct BeastBacking {
            const BeastFields* fields;
        };
        struct OwnedBacking {
            std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;
            explicit OwnedBacking(const std::pmr::polymorphic_allocator<> a)
                : entries(a) {
                // One up-front bump allocation (arena on the hot path) instead
                // of grow-reallocate-move on the first few add()s.
                entries.reserve(4);
            }
        };
        std::variant<BeastBacking, OwnedBacking> backing_;

        explicit Headers(BeastBacking b)
            : backing_{b} {
        }
        explicit Headers(OwnedBacking&& o)
            : backing_{std::move(o)} {
        }

        OwnedBacking& as_owned();  // asserts owned; used by mutators after promote
    };

}  // namespace menagerie::http
