#pragma once

#include <exception>

#include <boost/asio/awaitable.hpp>
#include <request_context.hpp>
#include <router.hpp>

namespace menagerie::http {

    /**
     * @brief Single typed observer interface: one class with all lifecycle
     *        and per-request hooks, rather than a separate callback vector
     *        per event.
     *
     * Lifecycle hooks: the awaitable ones run ON the injected executor -
     * on_setup_complete is AWAITED by setup() as a BARRIER before the accept
     * loops spawn (observer-owned resources exist before the first request;
     * the executor must already be driven by another thread);
     * on_shutdown_started is AWAITED by graceful_shutdown() phase 3, so real
     * async work (flush a buffer, close a pool) finishes before completion is
     * reported. on_shutdown_complete is sync + noexcept, fired right before
     * wait_until_stopped() unblocks. Observers are notified sequentially in
     * add order; a throwing async hook is caught and fanned to every
     * observer's on_unhandled_exception.
     *
     * Per-request hooks: fired from Router::dispatch through the Server-wired
     * std::function hooks - sync, noexcept, HOT PATH: keep them cheap and
     * allocation-free (the invocation itself is gated allocation-free).
     * on_response carries a RequestInfo SNAPSHOT: the RequestContext is
     * consumed by the handler chain, so it no longer exists when the response
     * is available; info's views stay valid through the hook call. Handler
     * throws surface via on_unhandled_exception (then the driver writes the
     * 500); driver-level early responses (malformed 400, limit 4xx) and the
     * synthesized 500 body are NOT observed.
     *
     * THREADING: per-request hooks and on_unhandled_exception may fire
     * concurrently from any executor thread (one strand per connection) -
     * implementations must be thread-safe. exception_ptr is heap-managed, so
     * there is no use-after-free hazard in passing it around.
     */
    class ServerObserver {
    public:
        virtual ~ServerObserver() = default;

        /// Awaited as a barrier by setup() before any accept loop spawns;
        /// override to bring up observer-owned resources first.
        virtual boost::asio::awaitable<void> on_setup_complete() {
            co_return;
        }
        /// Awaited by graceful_shutdown() phase 3; override to finish real
        /// async work (flush a buffer, close a pool) before shutdown reports complete.
        virtual boost::asio::awaitable<void> on_shutdown_started() {
            co_return;
        }
        /// Fired synchronously right before wait_until_stopped() unblocks.
        virtual void on_shutdown_complete() noexcept {
        }

        /// Hot-path hook: fired at dispatch entry, before routing.
        virtual void on_request([[maybe_unused]] const RequestContext& ctx) noexcept {
        }
        /// Hot-path hook: fired on handler success and on routing-miss
        /// 404/405; not fired when the handler throws.
        virtual void on_response([[maybe_unused]] const RequestInfo& info,
                                 [[maybe_unused]] const Response& resp) noexcept {
        }
        /// Hot-path hook: fired on a handler escape (the driver still writes the 500).
        virtual void on_unhandled_exception([[maybe_unused]] const std::exception_ptr& ep) noexcept {
        }
    };

}  // namespace menagerie::http
