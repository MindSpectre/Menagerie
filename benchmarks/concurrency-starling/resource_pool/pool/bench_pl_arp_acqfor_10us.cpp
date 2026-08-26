#include <chrono>
#include <cstddef>
#include <menagerie/starling>
#include <optional>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "async/async_bench_main.hpp"
#include "common/bench_scenarios.hpp"
#include "common/mock_resource.hpp"

using namespace bench::pool;
using RawPool = menagerie::starling::Pool<menagerie::starling::Wait::async, MockResource>;

// Adapter so run_coro_workload's measured region (`co_await pool.async_acquire_for`)
// stays byte-identical; only the pool type behind it changes.
struct PoolAdapter {
    RawPool pool;
    boost::asio::awaitable<std::optional<RawPool::Handle>> async_acquire_for(boost::asio::any_io_executor exec,
                                                                             std::chrono::nanoseconds timeout) {
        if (auto h = pool.try_acquire()) {
            co_return std::optional{std::move(*h)};
        }
        auto [ec, h] = co_await pool.acquire_for(exec, timeout, boost::asio::as_tuple(boost::asio::use_awaitable));
        if (ec) {
            co_return std::nullopt;
        }
        co_return std::optional{std::move(h)};
    }
};

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) {
        const std::size_t n = free_pool_size(sc, workers);
        std::size_t next    = 0;
        return PoolAdapter{
            RawPool{n, n, [next]() mutable -> std::optional<MockResource> { return MockResource{next++}; }}
        };
    };
    return run_async_bench_main<PoolAdapter>(
        argc, argv, "PL_Arp_AcqFor_10us", std::chrono::microseconds{10}, make_pool);
}
