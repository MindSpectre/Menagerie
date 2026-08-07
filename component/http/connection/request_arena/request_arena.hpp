#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <menagerie/beavers>

namespace menagerie::http {

    /**
     * @brief One heap block per CONNECTION, reused across every keep-alive
     *        request on it.
     *
     * Allocated once at `size` bytes (ServerConfig::request_arena_size(), default
     * 8 KB); reset() rewinds the monotonic resource to the initial block, so
     * the next request reuses the same memory - amortized, not per-request.
     * Requests that exceed the block grow via upstream new_delete blocks.
     *
     * Non-copyable AND non-movable: monotonic_buffer_resource is immovable, so
     * connections compose this by value and are constructed in place.
     */
    class RequestArena : beavers::Immutable {
    public:
        /// Allocates the single heap block, `size` bytes, that every
        /// allocation from this arena is bump-carved out of.
        explicit RequestArena(const std::size_t size = 8192)
            : initial_block_{std::make_unique<std::byte[]>(size)},
              resource_{initial_block_.get(), size} {
        }

        /// A pmr allocator bound to this arena's monotonic resource.
        [[nodiscard]] std::pmr::polymorphic_allocator<> allocator() noexcept {
            return std::pmr::polymorphic_allocator{&resource_};
        }

        /// Rewinds the arena to its initial block; does not free or shrink it.
        void reset() {
            resource_.release();  // rewinds to the initial block
        }

    private:
        std::unique_ptr<std::byte[]> initial_block_;
        std::pmr::monotonic_buffer_resource resource_;
    };

}  // namespace menagerie::http
