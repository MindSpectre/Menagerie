/**
 * @file
 * @brief Measures crow features that exist only on the sink-termination branch.
 *
 * Nothing here is comparable against the older branch -- these APIs do not exist there.
 * The point is to size the costs that the comparable benchmark cannot isolate, so they
 * can be subtracted from its deltas. The janitor in particular is unavoidable in the
 * comparable binary (health_check_interval has no counterpart on the other side), so
 * every delta that binary reports silently includes it; B1 is what makes it separable.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <menagerie/crow>
#include <ostream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include "benchmark_harness.hpp"

namespace {

    using menagerie::crow::ConsoleSink;
    using menagerie::crow::ConsoleSinkConfig;
    using menagerie::crow::FileSink;
    using menagerie::crow::FileSinkConfig;
    using menagerie::crow::LightEntry;
    using menagerie::crow::LogLevel;
    using menagerie::crow::Logger;
    using menagerie::crow::LoggerConfig;
    using menagerie::crow::Sink;
    using menagerie::crow::SinkStatus;

    constexpr std::size_t ITERATIONS = 4'000'000;
    constexpr std::size_t WARMUP     = ITERATIONS / 20;
    constexpr std::size_t PAD_SIZE   = bench::TARGET_RECORD_SIZE - 104;

    const std::string PADDING(PAD_SIZE, '0');

    /// Discards output while keeping the stream good(), so ConsoleSink never degrades.
    class NullBuf final : public std::streambuf {
    protected:
        std::streamsize xsputn(const char*, const std::streamsize count) override {
            return count;
        }
        int_type overflow(const int_type ch) override {
            return traits_type::not_eof(ch);
        }
    };

    struct Rig {
        std::unique_ptr<NullBuf> buf;
        std::unique_ptr<std::ostream> out;
        std::unique_ptr<Logger> logger;
        std::vector<std::shared_ptr<Sink>> sinks;
    };

    [[nodiscard]] LoggerConfig config_with(const std::chrono::milliseconds tick, const std::size_t pool) {
        return LoggerConfig::Builder{}
            .wait_strategy(LoggerConfig::WaitStrategy::Yielding)
            .ring_buffer_size(LoggerConfig::BufferCapacity::Medium)
            .pool_size(pool)
            .health_check_interval(tick)
            .finalize();
    }

    [[nodiscard]] std::shared_ptr<Sink> make_console(std::ostream* out) {
        return std::make_shared<ConsoleSink<LightEntry>>(ConsoleSinkConfig::Builder{}
                                                             .threshold(LogLevel::Debug)
                                                             .output(out)
                                                             .enable_colors(false)
                                                             .flush_each_entry(false)
                                                             .finalize());
    }

    [[nodiscard]] Rig make_rig(const std::chrono::milliseconds tick,
                               const std::size_t pool,
                               const std::size_t sink_count) {
        Rig rig{.buf = std::make_unique<NullBuf>(), .out = nullptr, .logger = nullptr, .sinks = {}};
        rig.out    = std::make_unique<std::ostream>(rig.buf.get());
        rig.logger = std::make_unique<Logger>(config_with(tick, pool));
        for (std::size_t i = 0; i < sink_count; ++i) {
            auto sink = make_console(rig.out.get());
            rig.logger->add_sink(sink);
            rig.sinks.push_back(std::move(sink));
        }
        return rig;
    }

    void log_once(Rig& rig) {
        rig.logger->log(LogLevel::Info, std::string_view{}, std::source_location::current(), "{}", PADDING);
    }

    /// Runs a scenario whose only variable is the Logger configuration.
    void config_scenario(const std::string& name,
                         const std::string& variant,
                         const std::vector<std::pair<std::string, std::string>>& params,
                         const std::chrono::milliseconds tick,
                         const std::size_t pool,
                         const std::size_t sink_count) {
        bench::ScenarioSpec spec{
            .name           = name,
            .variant        = variant,
            .threads        = bench::CONTENTION_THREADS,
            .iterations     = ITERATIONS,
            .warmup         = WARMUP,
            .timed_teardown = false,
            .params         = params,
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_rig(tick, pool, sink_count); },
            [](Rig& rig, std::size_t, std::size_t) { log_once(rig); },
            [](Rig& rig) {
                rig.logger->shutdown();
                return std::uint64_t{0};
            });

        bench::report(result);
    }

    /// B3: two sinks, one of them Dead, so the consumer's Dead-skip path is exercised.
    /// The sink's status is verified through sink_report() rather than assumed -- if the
    /// path were not blocked, this would silently measure two healthy sinks instead.
    void dead_sink_skip(const std::string& out_dir) {
        const std::filesystem::path blocker = std::filesystem::path{out_dir} / "blocked_sink_target";
        const std::filesystem::path dead_path = blocker / "dead.log";

        std::error_code ec;
        std::filesystem::remove_all(blocker, ec);
        std::ofstream{blocker} << "a regular file where a directory must be";

        bench::ScenarioSpec spec{
            .name           = "dead_sink_skip",
            .variant        = "console_plus_dead_file",
            .threads        = bench::CONTENTION_THREADS,
            .iterations     = ITERATIONS,
            .warmup         = WARMUP,
            .timed_teardown = false,
            .params         = {{"sinks", "1 healthy + 1 dead"}, {"wait", "Yielding"}},
        };
        if (!bench::selected(spec)) {
            std::filesystem::remove_all(blocker, ec);
            return;
        }

        bool verified = false;
        auto result   = bench::run_scenario(
            spec,
            [&] {
                Rig rig = make_rig(std::chrono::milliseconds{1000}, 2, 1);
                auto dead = std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                                        .threshold(LogLevel::Debug)
                                                                        .file(dead_path)
                                                                        .add_time_to_filename(false)
                                                                        .rotate_file(false)
                                                                        .finalize());
                rig.logger->add_sink(dead);
                rig.sinks.push_back(std::move(dead));

                for (const auto& report : rig.logger->sink_report()) {
                    if (report.status == SinkStatus::Dead) {
                        verified = true;
                    }
                }
                return rig;
            },
            [](Rig& rig, std::size_t, std::size_t) { log_once(rig); },
            [](Rig& rig) {
                rig.logger->shutdown();
                return std::uint64_t{0};
            });

        if (!verified) {
            std::cerr << "[FAIL] dead_sink_skip: no sink reported Dead -- the blocked path did not take effect, "
                      << "so this row would measure two healthy sinks. Not reported.\n";
        } else {
            bench::report(result);
        }
        std::filesystem::remove_all(blocker, ec);
    }

    /// B4: add_sink/remove_sink churn beneath steady logging, exercising the registry's
    /// copy-on-write path and the consumer's version check.
    void registry_churn(const int churn_hz) {
        bench::ScenarioSpec spec{
            .name           = "registry_churn",
            .variant        = "console_null",
            .threads        = bench::CONTENTION_THREADS,
            .iterations     = ITERATIONS,
            .warmup         = WARMUP,
            .timed_teardown = false,
            .params         = {{"churn_hz", std::to_string(churn_hz)}, {"wait", "Yielding"}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        std::atomic<bool> stop{false};
        std::thread churner;

        auto result = bench::run_scenario(
            spec,
            [&] {
                Rig rig = make_rig(std::chrono::milliseconds{1000}, 2, 1);
                stop.store(false, std::memory_order_relaxed);
                churner = std::thread{[&, logger = rig.logger.get(), out = rig.out.get()] {
                    const auto period = std::chrono::milliseconds{churn_hz > 0 ? 1000 / churn_hz : 1000};
                    while (!stop.load(std::memory_order_relaxed)) {
                        auto sink = make_console(out);
                        logger->add_sink(sink);
                        std::this_thread::sleep_for(period);
                        logger->remove_sink(sink);
                    }
                }};
                return rig;
            },
            [](Rig& rig, std::size_t, std::size_t) { log_once(rig); },
            [&](Rig& rig) {
                stop.store(true, std::memory_order_relaxed);
                if (churner.joinable()) {
                    churner.join();
                }
                rig.logger->shutdown();
                return std::uint64_t{0};
            });

        bench::report(result);
    }

    /// B5: repeated sweep() against a wide registry, which republishes the gate under
    /// registry_mutex_ on every pass.
    void gate_recompute() {
        bench::ScenarioSpec spec{
            .name           = "gate_recompute",
            .variant        = "8_sinks",
            .threads        = bench::CONTENTION_THREADS,
            .iterations     = ITERATIONS,
            .warmup         = WARMUP,
            .timed_teardown = false,
            .params         = {{"sinks", "8"}, {"sweep_hz", "100"}, {"wait", "Yielding"}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        std::atomic<bool> stop{false};
        std::thread sweeper;

        auto result = bench::run_scenario(
            spec,
            [&] {
                Rig rig = make_rig(std::chrono::milliseconds{0}, 2, 8);
                stop.store(false, std::memory_order_relaxed);
                sweeper = std::thread{[&, logger = rig.logger.get()] {
                    while (!stop.load(std::memory_order_relaxed)) {
                        logger->sweep();
                        std::this_thread::sleep_for(std::chrono::milliseconds{10});
                    }
                }};
                return rig;
            },
            [](Rig& rig, std::size_t, std::size_t) { log_once(rig); },
            [&](Rig& rig) {
                stop.store(true, std::memory_order_relaxed);
                if (sweeper.joinable()) {
                    sweeper.join();
                }
                rig.logger->shutdown();
                return std::uint64_t{0};
            });

        bench::report(result);
    }

}  // namespace

int main(const int argc, char** argv) {
    bench::parse_args(argc, argv);
    bench::env_check();

    std::cout << "\ncrow termination benchmark -- branch-only features\n"
              << "  reps: " << bench::run_context().reps << "\n";

    // B1: the janitor's isolated cost, so it can be subtracted from the comparable
    // binary's deltas (which always carry it -- the other branch has no way to disable it).
    config_scenario("janitor_cost", "off", {{"health_check_interval_ms", "0"}, {"sinks", "1"}},
                    std::chrono::milliseconds{0}, 2, 1);
    config_scenario("janitor_cost", "default", {{"health_check_interval_ms", "1000"}, {"sinks", "1"}},
                    std::chrono::milliseconds{1000}, 2, 1);
    config_scenario("janitor_cost", "aggressive", {{"health_check_interval_ms", "100"}, {"sinks", "1"}},
                    std::chrono::milliseconds{100}, 2, 1);

    // B2: justifies the pool_size default moving from hardware_concurrency() to 2.
    for (const std::size_t pool : {std::size_t{1}, std::size_t{2}, std::size_t{12}}) {
        config_scenario("pool_size", std::format("pool{}_1sink", pool),
                        {{"pool_size", std::to_string(pool)}, {"sinks", "1"}},
                        std::chrono::milliseconds{1000}, pool, 1);
        config_scenario("pool_size", std::format("pool{}_2sinks", pool),
                        {{"pool_size", std::to_string(pool)}, {"sinks", "2"}},
                        std::chrono::milliseconds{1000}, pool, 2);
    }

    dead_sink_skip(bench::run_context().out_dir);

    for (const int hz : {1, 10, 100}) {
        registry_churn(hz);
    }

    gate_recompute();

    std::cout << "\ndone\n";
    return 0;
}
