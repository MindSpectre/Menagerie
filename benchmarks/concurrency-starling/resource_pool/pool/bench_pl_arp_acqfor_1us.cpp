#include <chrono>
#include <cstddef>

#include "async/async_bench_main.hpp"
#include "common/bench_scenarios.hpp"
#include "populated_pool.hpp"

using namespace bench::pool;
using PoolT = PopulatedPool;

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) { return PoolT{free_pool_size(sc, workers)}; };
    return run_async_bench_main<PoolT>(argc, argv, "PL_Arp_AcqFor_1us", std::chrono::microseconds{1}, make_pool);
}
