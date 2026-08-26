#include <chrono>
#include <cstddef>
#include <menagerie/starling>
#include <optional>

#include "common/bench_scenarios.hpp"
#include "common/mock_resource.hpp"
#include "sync/sync_bench_main.hpp"

using namespace bench::pool;
using PoolT = menagerie::starling::Pool<menagerie::starling::Wait::sync, MockResource>;

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) {
        const std::size_t n = free_pool_size(sc, workers);
        std::size_t next    = 0;
        // min_eager == capacity: fully eager, matching the old pools' construction
        // so the measured workload sees an identical starting state.
        return PoolT{n, n, [next]() mutable -> std::optional<MockResource> { return MockResource{next++}; }};
    };
    auto strategy = [](PoolT& p) { return p.acquire_for(std::chrono::microseconds{2}); };
    return run_sync_bench_main<PoolT>(argc, argv, "PL_AcqFor_2us", FREE_REGION_SCENARIOS, make_pool, strategy);
}
