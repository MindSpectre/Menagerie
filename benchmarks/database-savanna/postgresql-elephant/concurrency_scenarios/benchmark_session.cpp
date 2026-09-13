#include <cstddef>
#include <format>
#include <memory>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <postgres_session.hpp>

#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_scenarios.hpp"
#include "bench_workload.hpp"

namespace {

    using menagerie::savanna::elephant::ConnectionConfig;
    using menagerie::savanna::elephant::PoolConfig;
    using menagerie::savanna::elephant::Session;

    /// Which acquisition verb the workload exercises on the one Session.
    enum class AcquireMode {
        Try,    ///< try_with_async: fail-fast, sheds load on exhaustion.
        Await,  ///< co_await with_async: parks the coroutine on the FIFO queue.
    };

    class SessionBackend {
    public:
        SessionBackend(const bench::pg::Scenario& scenario, const AcquireMode mode)
            : session_{ConnectionConfig::testing(), make_pool_config(scenario)},
              work_guard_{boost::asio::make_work_guard(io_)},
              mode_{mode} {
            io_threads_.reserve(scenario.executor_threads);
            for (std::size_t i = 0; i < scenario.executor_threads; ++i) {
                io_threads_.emplace_back([this] { io_.run(); });
            }
            query_ = bench::pg::query_for(scenario);
        }

        ~SessionBackend() {
            if (running_) {
                wait_and_shutdown();
            }
        }

        SessionBackend(const SessionBackend&)            = delete;
        SessionBackend& operator=(const SessionBackend&) = delete;

        boost::asio::io_context& io() noexcept {
            return io_;
        }

        boost::asio::awaitable<bool> run_query(int id, boost::asio::any_io_executor e) {
            auto acquired = mode_ == AcquireMode::Try ? session_.try_with_async(e) : co_await session_.with_async(e);
            if (!acquired.has_value()) {
                co_return false;
            }
            auto ae     = std::move(acquired).value();
            auto result = co_await ae.execute(std::string{query_}, id);
            benchmark::DoNotOptimize(result);
            co_return result.has_value();
        }

        void wait_and_shutdown() {
            if (!running_) {
                return;
            }
            running_ = false;
            work_guard_.reset();
            io_.stop();
            for (auto& t : io_threads_) {
                if (t.joinable()) {
                    t.join();
                }
            }
            session_.shutdown();
        }

    private:
        static PoolConfig make_pool_config(const bench::pg::Scenario& s) {
            return PoolConfig::Builder{}.capacity(s.connections).min_connections(s.connections).finalize();
        }

        Session session_;
        boost::asio::io_context io_{};
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
        std::vector<std::thread> io_threads_{};
        const char* query_ = bench::pg::BENCH_QUERY;
        AcquireMode mode_;
        bool running_ = true;
    };

    void RunScenario(::benchmark::State& state, const std::size_t scenario_index, const AcquireMode mode) {
        const auto& scenario = bench::pg::SCENARIOS[scenario_index];
        SessionBackend backend{scenario, mode};
        bench::pg::run_async_workload(state, scenario, backend);
        backend.wait_and_shutdown();
    }

    int RegisterAll() {
        for (std::size_t i = 0; i < bench::pg::SCENARIOS.size(); ++i) {
            const auto try_name = std::format("BM_SessionTry_{}", bench::pg::SCENARIOS[i].name);
            auto* t             = ::benchmark::RegisterBenchmark(
                try_name, [i](::benchmark::State& st) { RunScenario(st, i, AcquireMode::Try); });
            bench::pg::register_workers(t);

            const auto await_name = std::format("BM_SessionAwait_{}", bench::pg::SCENARIOS[i].name);
            auto* a               = ::benchmark::RegisterBenchmark(
                await_name, [i](::benchmark::State& st) { RunScenario(st, i, AcquireMode::Await); });
            bench::pg::register_workers(a);
        }
        return 0;
    }

    const int kRegistered = RegisterAll();

}  // namespace

int main(int argc, char** argv) {
    if (auto* c = bench::pg::connect(); c != nullptr) {
        bench::pg::setup_table(c);
        PQfinish(c);
    }

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    if (auto* c = bench::pg::connect(); c != nullptr) {
        bench::pg::teardown_table(c);
        PQfinish(c);
    }
    return 0;
}
