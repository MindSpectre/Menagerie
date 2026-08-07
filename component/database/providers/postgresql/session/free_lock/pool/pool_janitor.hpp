#pragma once

#include <thread>

#include "connection_pool.hpp"

namespace menagerie::db::postgres {

    /**
     * @brief Background health monitor for the connection pool
     *
     * Periodically sweeps the ring buffer to:
     *   - Replace DEAD slots with a freshly created connection (or leave INACTIVE
     *     if the replacement attempt also fails)
     *   - Health-check every FREE slot's connection and mark it DEAD if it is no
     *     longer usable, so the next sweep replaces it
     *
     * Uses std::jthread with stop_token for immediate cancellation
     * without spinning or sleeping through the full interval.
     */
    class PoolJanitor : beavers::Immutable {
    public:
        /**
         * @brief Starts the background sweep thread immediately
         * @param pool Pool to sweep; must outlive this janitor.
         * @throw std::system_error if the OS fails to create the sweep thread.
         */
        explicit PoolJanitor(ConnectionPool& pool);

        /// Stops and joins the sweep thread (equivalent to calling stop()).
        ~PoolJanitor();

        /// Requests the sweep thread to stop and joins it. Idempotent.
        void stop();

    private:
        void run(const std::stop_token& token) const;
        void sweep(const std::stop_token& token) const;

        ConnectionPool& pool_;
        std::jthread thread_;
    };

}  // namespace menagerie::db::postgres
