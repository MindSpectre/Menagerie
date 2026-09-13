#pragma once

#include <expected>
#include <menagerie/beaver>

#include <postgres_errors.hpp>

#include "postgres_async_executor.hpp"
#include "postgres_sync_executor.hpp"

namespace menagerie::savanna {

    /**
     * @brief Concept satisfied by types that can lend out a synchronous query executor
     *        immediately, without waiting.
     *
     * Modeled structurally by exposing with_sync(), returning
     * std::expected<SyncExecutor, ErrorContext>.
     */
    template <typename T>
    concept SyncCapabilityProvider = requires(T provider) {
        { provider.with_sync() } -> std::same_as<std::expected<elephant::SyncExecutor, elephant::ErrorContext>>;
    };

    /**
     * @brief Concept satisfied by types that can lend out an asynchronous query executor
     *        immediately, without waiting.
     *
     * Modeled structurally by exposing with_async(exec), returning
     * std::expected<AsyncExecutor, ErrorContext> synchronously (the executor's
     * own operations are what suspend, not the borrow).
     */
    template <typename T>
    concept AsyncCapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_async(exec) } -> std::same_as<std::expected<elephant::AsyncExecutor, elephant::ErrorContext>>;
    };

    /**
     * @brief Concept satisfied by types that lend out both executor kinds immediately.
     *
     * Transaction and AutoTransaction model the full concept: their connection is
     * already borrowed, so both with_sync() and with_async(exec) complete without
     * waiting. Session models only SyncCapabilityProvider - its with_async(exec)
     * overloads return an awaitable that suspends the caller until a connection
     * frees, and its immediate async borrow is spelled try_with_async(exec). This
     * is a concept, not a base class, so conforming types opt in structurally
     * rather than through inheritance.
     */
    template <typename T>
    concept CapabilityProvider = SyncCapabilityProvider<T> && AsyncCapabilityProvider<T>;

}  // namespace menagerie::savanna
