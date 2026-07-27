#include <filesystem>
#include <menagerie/crow>
#include <source_location>

#include "benchmark_harness.hpp"

namespace {

    class NullSink final : public menagerie::crow::Sink {
    public:
        void process(const menagerie::crow::LogEvent& /*event*/) override {
        }
        void flush() override {
        }
        [[nodiscard]] bool should_log(menagerie::crow::LogLevel /*lvl*/,
                                      std::string_view /*prefix*/) const noexcept override {
            return true;
        }
    };

    constexpr std::size_t CROW_PREFIX_EST = 104;
    constexpr std::size_t CROW_PAD_SIZE   = bench::TARGET_RECORD_SIZE - CROW_PREFIX_EST;

    auto make_logger() {
        return std::make_unique<menagerie::crow::Logger>(
            menagerie::crow::LoggerConfig::Builder{}
                .wait_strategy(menagerie::crow::LoggerConfig::WaitStrategy::BusySpin)
                .ring_buffer_size(menagerie::crow::LoggerConfig::BufferCapacity::Medium)
                .finalize());
    }

}  // namespace

int main() {
    const std::string padding(CROW_PAD_SIZE, '0');

    std::cout << "Scroll File Logger Benchmark\n";
    std::cout << "Padding size: " << CROW_PAD_SIZE << " bytes (target record ~" << bench::TARGET_RECORD_SIZE
              << " bytes)\n";

    // Test 1: 8-thread file sink contention
    {
        std::filesystem::remove("crow_contention_8t.log");

        menagerie::crow::FileSinkConfig sink_config =
            menagerie::crow::FileSinkConfig::Builder{}
                .threshold(menagerie::crow::LogLevel::Debug)
                .file("crow_contention_8t.log")
                .add_time_to_filename(false)
                .max_file_size(menagerie::beavers::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotate_file(false)
                .finalize();

        auto file_sink = std::make_shared<menagerie::crow::FileSink<menagerie::crow::DetailedEntry>>(sink_config);
        auto logger    = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(
                menagerie::crow::LogLevel::Debug, std::string_view{}, std::source_location::current(), "{}", padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread Contention");
    }

    // Test 2: 1-thread file sink baseline
    {
        std::filesystem::remove("crow_baseline_1t.log");

        menagerie::crow::FileSinkConfig sink_config =
            menagerie::crow::FileSinkConfig::Builder{}
                .threshold(menagerie::crow::LogLevel::Debug)
                .file("crow_baseline_1t.log")
                .add_time_to_filename(false)
                .max_file_size(menagerie::beavers::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotate_file(false)
                .finalize();

        auto file_sink = std::make_shared<menagerie::crow::FileSink<menagerie::crow::DetailedEntry>>(sink_config);
        auto logger    = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark(bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(
                menagerie::crow::LogLevel::Debug, std::string_view{}, std::source_location::current(), "{}", padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "1-Thread Baseline");
    }

    // Test 3: 8-thread null sink
    {
        auto null_sink = std::make_shared<NullSink>();
        auto logger    = make_logger();
        logger->add_sink(null_sink);

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(
                menagerie::crow::LogLevel::Debug, std::string_view{}, std::source_location::current(), "{}", padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread NullSink");
    }

    // Test 4: 1-thread null sink
    {
        auto null_sink = std::make_shared<NullSink>();
        auto logger    = make_logger();
        logger->add_sink(null_sink);

        auto result = bench::run_threaded_benchmark(bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(
                menagerie::crow::LogLevel::Debug, std::string_view{}, std::source_location::current(), "{}", padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "1-Thread NullSink");
    }

    // Test 5: 8-thread dual file sink (async parallelism test)
    {
        std::filesystem::remove("crow_dual_a.log");
        std::filesystem::remove("crow_dual_b.log");

        auto make_file_sink = [](const std::string& path) {
            return std::make_shared<menagerie::crow::FileSink<menagerie::crow::DetailedEntry>>(
                menagerie::crow::FileSinkConfig::Builder{}
                    .threshold(menagerie::crow::LogLevel::Debug)
                    .file(path)
                    .add_time_to_filename(false)
                    .max_file_size(menagerie::beavers::literals::operator""_mb(500))
                    .flush_each_entry(false)
                    .rotate_file(false)
                    .finalize());
        };

        auto logger = make_logger();
        logger->add_sink(make_file_sink("crow_dual_a.log"));
        logger->add_sink(make_file_sink("crow_dual_b.log"));

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(
                menagerie::crow::LogLevel::Debug, std::string_view{}, std::source_location::current(), "{}", padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread Dual FileSink");
    }

    // Test 6: 8-thread E2E (wall-clock includes shutdown/strand drain)
    {
        std::filesystem::remove("crow_e2e_8t.log");

        auto file_sink = std::make_shared<menagerie::crow::FileSink<menagerie::crow::DetailedEntry>>(
            menagerie::crow::FileSinkConfig::Builder{}
                .threshold(menagerie::crow::LogLevel::Debug)
                .file("crow_e2e_8t.log")
                .add_time_to_filename(false)
                .max_file_size(menagerie::beavers::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotate_file(false)
                .finalize());

        auto logger = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark_e2e(
            bench::CONTENTION_THREADS,
            bench::ITERATIONS_PER_THREAD,
            [&] {
                logger->log(menagerie::crow::LogLevel::Debug,
                            std::string_view{},
                            std::source_location::current(),
                            "{}",
                            padding);
            },
            [&] { logger->shutdown(); });
        bench::print_results(result, "Scroll", "8-Thread E2E (incl. shutdown)");
    }

    // Test 7: 8-thread dual file sink E2E (incl. shutdown)
    {
        std::filesystem::remove("crow_e2e_dual_a.log");
        std::filesystem::remove("crow_e2e_dual_b.log");

        auto make_file_sink = [](const std::string& path) {
            return std::make_shared<menagerie::crow::FileSink<menagerie::crow::DetailedEntry>>(
                menagerie::crow::FileSinkConfig::Builder{}
                    .threshold(menagerie::crow::LogLevel::Debug)
                    .file(path)
                    .add_time_to_filename(false)
                    .max_file_size(menagerie::beavers::literals::operator""_mb(500))
                    .flush_each_entry(false)
                    .rotate_file(false)
                    .finalize());
        };

        auto logger = make_logger();
        logger->add_sink(make_file_sink("crow_e2e_dual_a.log"));
        logger->add_sink(make_file_sink("crow_e2e_dual_b.log"));

        auto result = bench::run_threaded_benchmark_e2e(
            bench::CONTENTION_THREADS,
            bench::ITERATIONS_PER_THREAD,
            [&] {
                logger->log(menagerie::crow::LogLevel::Debug,
                            std::string_view{},
                            std::source_location::current(),
                            "{}",
                            padding);
            },
            [&] { logger->shutdown(); });
        bench::print_results(result, "Scroll", "8-Thread Dual E2E (incl. shutdown)");
    }

    return 0;
}
