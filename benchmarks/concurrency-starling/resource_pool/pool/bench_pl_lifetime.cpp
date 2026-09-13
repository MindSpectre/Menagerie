#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

#include "async/coro_workload.hpp"
#include "common/bench_latency.hpp"
#include "common/bench_scenarios.hpp"
#include "common/pool_bench_main.hpp"
#include "pool/populated_pool.hpp"
#include "sync/bench_workload.hpp"

namespace bench::pool {
    namespace {
        using menagerie::starling::AcquireError;

        constexpr std::size_t SAMPLE_BUDGET = 1u << 20;

        inline constexpr Scenario POOL_HANDOFF{
            "PoolHandoff",
            ScenarioKind::HeavyBurst,
            std::chrono::nanoseconds{500},
            1,
            std::chrono::nanoseconds{0},
        };
        inline constexpr std::size_t POOL_HANDOFF_WORKERS = 8;

        class BoundedLatencyRegistry {
        public:
            explicit BoundedLatencyRegistry(const std::size_t collector_count)
                : collectors_(std::max<std::size_t>(collector_count, 1)),
                  per_collector_limit_(std::max<std::size_t>(SAMPLE_BUDGET / collectors_.size(), 1)),
                  generation_(next_generation_.fetch_add(1, std::memory_order_relaxed)) {
                for (auto& collector : collectors_) {
                    collector.reserve(per_collector_limit_);
                }
            }

            void record(const std::chrono::nanoseconds latency) {
                auto& collector = local_collector();
                if (collector.samples.size() < per_collector_limit_) {
                    collector.record(latency);
                }
            }

            void merge_into(benchmark::State& state) const {
                merge_latency(state, collectors_);
            }

        private:
            LatencyCollector& local_collector() {
                static thread_local std::uint64_t local_generation = 0;
                static thread_local std::size_t local_index        = 0;
                if (local_generation != generation_) {
                    local_index      = next_collector_.fetch_add(1, std::memory_order_relaxed);
                    local_generation = generation_;
                }
                if (local_index >= collectors_.size()) {
                    throw std::logic_error{"lifetime benchmark collector registration overflow"};
                }
                return collectors_[local_index];
            }

            std::vector<LatencyCollector> collectors_;
            std::size_t per_collector_limit_;
            std::atomic<std::size_t> next_collector_{0};
            std::uint64_t generation_;
            inline static std::atomic<std::uint64_t> next_generation_{1};
        };

        template <typename Borrowed>
        class TimedBorrowed {
        public:
            TimedBorrowed(Borrowed borrowed,
                          const std::uint64_t start_cycles,
                          BoundedLatencyRegistry& registry) noexcept
                : borrowed_{std::move(borrowed)},
                  start_cycles_{start_cycles},
                  registry_{&registry} {
            }

            TimedBorrowed(TimedBorrowed&& other) noexcept
                : borrowed_{std::move(other.borrowed_)},
                  start_cycles_{other.start_cycles_},
                  registry_{std::exchange(other.registry_, nullptr)} {
            }

            TimedBorrowed& operator=(TimedBorrowed&& other) noexcept {
                if (this != &other) {
                    finish();
                    borrowed_     = std::move(other.borrowed_);
                    start_cycles_ = other.start_cycles_;
                    registry_     = std::exchange(other.registry_, nullptr);
                }
                return *this;
            }

            TimedBorrowed(const TimedBorrowed&)            = delete;
            TimedBorrowed& operator=(const TimedBorrowed&) = delete;

            ~TimedBorrowed() noexcept {
                finish();
            }

            [[nodiscard]] auto* get() noexcept {
                return borrowed_ ? borrowed_->get() : nullptr;
            }

            [[nodiscard]] auto* operator->() noexcept {
                return borrowed_->get();
            }

        private:
            void finish() noexcept {
                if (registry_ == nullptr) {
                    return;
                }
                // Return the pool slot before taking the end timestamp. This makes
                // every sample a complete checkout/use/return lifecycle.
                borrowed_.reset();
                const std::uint64_t end_cycles = TscClock::now();
                registry_->record(TscClock::to_duration(end_cycles - start_cycles_));
                registry_ = nullptr;
            }

            std::optional<Borrowed> borrowed_;
            std::uint64_t start_cycles_;
            BoundedLatencyRegistry* registry_;
        };

        class TimedPool {
        public:
            using InnerPool     = PopulatedPool;
            using InnerBorrowed = typename InnerPool::Borrowed;
            using Borrowed      = TimedBorrowed<InnerBorrowed>;

            TimedPool(const std::size_t capacity, const std::size_t collector_count)
                : inner_{capacity},
                  registry_{collector_count} {
            }

            [[nodiscard]] std::expected<Borrowed, AcquireError> try_acquire() {
                const std::uint64_t start = TscClock::now();
                return wrap(inner_.try_acquire(), start);
            }

            [[nodiscard]] std::expected<Borrowed, AcquireError> acquire_for(const std::chrono::nanoseconds timeout) {
                const std::uint64_t start = TscClock::now();
                return wrap(inner_.acquire_for(timeout), start);
            }

            boost::asio::awaitable<std::expected<Borrowed, AcquireError>>
            async_acquire_for(boost::asio::any_io_executor executor, const std::chrono::nanoseconds timeout) {
                const std::uint64_t start = TscClock::now();
                auto result               = co_await inner_.async_acquire_for(std::move(executor), timeout);
                co_return wrap(std::move(result), start);
            }

            void merge_lifecycle_latency(benchmark::State& state) const {
                registry_.merge_into(state);
            }

        private:
            std::expected<Borrowed, AcquireError> wrap(std::expected<InnerBorrowed, AcquireError> result,
                                                       const std::uint64_t start) {
                if (!result) {
                    return std::unexpected{result.error()};
                }
                return Borrowed{std::move(*result), start, registry_};
            }

            InnerPool inner_;
            BoundedLatencyRegistry registry_;
        };

        void checked_registration(benchmark::Benchmark* registration, const std::string& name) {
            if (registration == nullptr) {
                throw std::runtime_error{"failed to register benchmark " + name};
            }
        }

        void register_try_subject() {
            const std::string name = "BM_PoolLifecycle_Try";
            auto* registration     = benchmark::RegisterBenchmark(name, [](benchmark::State& state) {
                PopulatedPool pool{128};
                benchmark::DoNotOptimize(pool.raw());
                benchmark::ClobberMemory();
                std::vector<LatencyCollector> collectors(1);
                collectors.front().reserve(SAMPLE_BUDGET);
                std::int64_t completed = 0;
                for (auto _ : state) {  // NOLINT(clang-diagnostic-unused-but-set-variable)
                    (void)_;
                    const std::uint64_t start = TscClock::now();
                    {
                        auto resource = pool.try_acquire();
                        benchmark::DoNotOptimize(resource.has_value());
                        if (resource) {
                            benchmark::DoNotOptimize(resource->get());
                            ++completed;
                        }
                    }
                    const std::uint64_t end = TscClock::now();
                    if (collectors.front().samples.size() < SAMPLE_BUDGET) {
                        collectors.front().record(TscClock::to_duration(end - start));
                    }
                }
                state.SetItemsProcessed(completed);
                merge_latency(state, collectors);
            });
            checked_registration(registration, name);
            registration->UseRealTime();
        }

        void register_try_throughput_control() {
            const std::string name = "BM_PoolThroughput_Try";
            auto* registration     = benchmark::RegisterBenchmark(name, [](benchmark::State& state) {
                PopulatedPool pool{128};
                benchmark::DoNotOptimize(pool.raw());
                benchmark::ClobberMemory();
                std::int64_t completed = 0;
                for (auto _ : state) {  // NOLINT(clang-diagnostic-unused-but-set-variable)
                    (void)_;
                    auto resource = pool.try_acquire();
                    benchmark::DoNotOptimize(resource.has_value());
                    if (resource) {
                        benchmark::DoNotOptimize(resource->get());
                        ++completed;
                    }
                    // `resource` returns here, inside the Google Benchmark timed
                    // region. There are no per-operation TSC reads or samples.
                }
                state.SetItemsProcessed(completed);
            });
            checked_registration(registration, name);
            registration->UseRealTime();
        }

        void register_sync_subject(const ScenarioKind kind, const bool pin) {
            const Scenario& workload = scenario(kind);
            const std::string name =
                "BM_PoolLifecycle_" + std::string{workload.name};
            auto* registration = benchmark::RegisterBenchmark(name, [workload, pin](benchmark::State& state) {
                const auto workers = static_cast<std::size_t>(state.range(0));
                TimedPool pool{FREE_POOL_SIZE, workers};
                auto strategy = [](auto& candidate) { return candidate.acquire_for(std::chrono::seconds{1}); };
                run_workload(state, workload, pool, strategy, pin);
                pool.merge_lifecycle_latency(state);
            });
            checked_registration(registration, name);
            if (pin) {
                register_pinned_worker(registration);
            } else {
                register_floating_workers(registration);
            }
        }

        void register_sync_handoff(const bool pin) {
            const std::string name = "BM_PoolLifecycle_SyncHandoff";
            auto* registration     = benchmark::RegisterBenchmark(name, [pin](benchmark::State& state) {
                TimedPool pool{1, POOL_HANDOFF_WORKERS};
                auto strategy = [](auto& candidate) { return candidate.acquire_for(std::chrono::seconds{1}); };
                run_workload(state, POOL_HANDOFF, pool, strategy, pin);
                pool.merge_lifecycle_latency(state);
            });
            checked_registration(registration, name);
            registration->Arg(static_cast<std::int64_t>(POOL_HANDOFF_WORKERS))->UseRealTime();
        }

        void register_async_handoff(const bool pin) {
            const std::string name = "BM_PoolLifecycle_AsyncHandoff";
            auto* registration     = benchmark::RegisterBenchmark(name, [pin](benchmark::State& state) {
                TimedPool pool{1, CORE_COUNT};
                run_coro_workload(state, POOL_HANDOFF, pool, std::chrono::seconds{1}, pin);
                pool.merge_lifecycle_latency(state);
            });
            checked_registration(registration, name);
            registration->Arg(static_cast<std::int64_t>(POOL_HANDOFF_WORKERS))->UseRealTime();
        }

        void register_pool_workloads(const bool pin) {
            register_try_throughput_control();
            register_try_subject();
            for (const auto workload :
                 {ScenarioKind::Steady, ScenarioKind::TimeoutPressure, ScenarioKind::HeavyBurst}) {
                register_sync_subject(workload, pin);
            }
            register_sync_handoff(pin);
            register_async_handoff(pin);
        }
    }  // namespace
}  // namespace bench::pool

int main(int argc, char** argv) {
    const bool pin = bench::pool::parse_pin_flag(argc, argv);
    bench::pool::print_calibration();
    bench::pool::env_check();
    bench::pool::register_pool_workloads(pin);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        benchmark::Shutdown();
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
