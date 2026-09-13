#include <chrono>
#include <cstddef>

#include "common/bench_scenarios.hpp"
#include "populated_pool.hpp"
#include "sync/sync_bench_main.hpp"

using namespace bench::pool;
using PoolT = PopulatedPool;

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) { return PoolT{free_pool_size(sc, workers)}; };
    auto strategy  = [](PoolT& p) { return p.acquire_for(std::chrono::microseconds{1}); };
    return run_sync_bench_main<PoolT>(argc, argv, "PL_AcqFor_1us", FREE_REGION_SCENARIOS, make_pool, strategy);
}
