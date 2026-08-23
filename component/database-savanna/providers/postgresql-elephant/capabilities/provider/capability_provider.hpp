#pragma once

#include <expected>
#include <menagerie/beaver>

#include <postgres_errors.hpp>

#include "postgres_async_executor.hpp"
#include "postgres_sync_executor.hpp"

namespace menagerie::savanna {
    /**
     * @brief Concept satisfied by session types that can lend out query executors.
     *
     * A type models CapabilityProvider by exposing with_sync(), returning
     * std::expected<SyncExecutor, ErrorContext>, and with_async(exec), returning
     * std::expected<AsyncExecutor, ErrorContext>. This is a concept, not a base class, so
     * conforming session types opt in structurally rather than through inheritance.
     */
    template <typename T>
    concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_sync() } -> std::same_as<std::expected<elephant::SyncExecutor, elephant::ErrorContext>>;
        { provider.with_async(exec) } -> std::same_as<std::expected<elephant::AsyncExecutor, elephant::ErrorContext>>;
    };

}  // namespace menagerie::savanna
