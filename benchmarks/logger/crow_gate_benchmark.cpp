/**
 * @file
 * @brief Skip-rate sweep comparing crow between two branches.
 *
 * This binary is the A/B subject: it is built from identical source on both sides, so
 * every delta it reports is attributable to the crow library rather than to the
 * benchmark. It therefore uses ONLY APIs that exist on both branches -- no SinkStatus,
 * no remove_sink, no sink_report, no sweep, no health_check_interval. Branch-only
 * measurements live in crow_termination_benchmark.cpp, which the older branch does not
 * build.
 *
 * The sweep axis is the fraction of log() calls that no sink accepts. At 0.00 the newer
 * branch pays for gating machinery it never uses; at 1.00 it skips formatting entirely.
 * Reporting only one end would be unfalsifiable, so both ends are measured.
 *
 * The gating sink must be a LIBRARY sink. Sink::publish_threshold() is protected and is
 * called only from ConsoleSink's and FileSink's constructors, so a benchmark-local sink
 * would keep the default threshold (Trace), hold the gate open, and report "no benefit"
 * as an artifact of the benchmark rather than a property of the library.
 */

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <menagerie/crow>
#include <ostream>
#include <random>
#include <streambuf>
#include <string>
#include <vector>

#include "benchmark_harness.hpp"

namespace {

    using menagerie::crow::ConsoleSink;
    using menagerie::crow::ConsoleSinkConfig;
    using menagerie::crow::DetailedEntry;
    using menagerie::crow::FileSink;
    using menagerie::crow::FileSinkConfig;
    using menagerie::crow::LightEntry;
    using menagerie::crow::LogLevel;
    using menagerie::crow::Logger;
    using menagerie::crow::LoggerConfig;
    using menagerie::crow::PrefixFilter;

    // -------- Fixed measurement parameters --------

    /// Iterations are compile-time constants, identical on both sides, never calibrated
    /// per side. They are sized so the SLOWER side runs for a few seconds; the faster
    /// side is correspondingly shorter, which is fine because thread spawn now sits
    /// outside the timed region and join costs tens of microseconds.
    constexpr std::size_t ITER_8T   = 4'000'000;  // 32M attempted calls
    constexpr std::size_t ITER_1T   = 8'000'000;
    constexpr std::size_t ITER_FILE = 1'000'000;  // 8M records x ~256 B is already ~2 GB

    constexpr std::size_t WARMUP_8T   = ITER_8T / 20;
    constexpr std::size_t WARMUP_1T   = ITER_1T / 20;
    constexpr std::size_t WARMUP_FILE = ITER_FILE / 20;

    /// Sized so a record lands near 256 bytes once DetailedEntry's prefix is added.
    constexpr std::size_t CROW_PREFIX_EST = 104;
    constexpr std::size_t PAD_SIZE        = bench::TARGET_RECORD_SIZE - CROW_PREFIX_EST;

    /// The level a sink accepts from. Debug is below it (skipped), Warning is at it
    /// (passes). Both render as three characters, so record size does not move with the
    /// skip rate.
    constexpr LogLevel THRESHOLD = LogLevel::Warning;
    constexpr LogLevel SKIPPED   = LogLevel::Debug;
    constexpr LogLevel PASSING   = LogLevel::Warning;

    /// 8192 entries so the blocked variant can hold runs of 4096 -- long enough that a
    /// consumer batch is often entirely skippable, which is what the per-batch sink skip
    /// needs in order to fire at all.
    constexpr std::size_t TABLE_SIZE = 8192;
    constexpr std::size_t TABLE_MASK = TABLE_SIZE - 1;

    using LevelTable = std::array<LogLevel, TABLE_SIZE>;

    // -------- Counting null sink target --------

    /**
     * @brief A streambuf that discards output while counting it.
     *
     * Two invariants matter. It must count one record per ConsoleSink write, which holds
     * because operator<<(ostream&, const string&) issues exactly one sputn. And it must
     * leave the stream good(): if xsputn returned short or overflow returned eof, the
     * newer branch's ConsoleSink would call mark_degraded() on every single event, taking
     * a mutex each time -- a confound large enough to invert the result.
     */
    class CountingNullBuf final : public std::streambuf {
    public:
        [[nodiscard]] std::uint64_t records() const noexcept {
            return records_.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t bytes() const noexcept {
            return bytes_.load(std::memory_order_relaxed);
        }

    protected:
        std::streamsize xsputn(const char*, const std::streamsize count) override {
            records_.fetch_add(1, std::memory_order_relaxed);
            bytes_.fetch_add(static_cast<std::uint64_t>(count), std::memory_order_relaxed);
            return count;
        }

        int_type overflow(const int_type ch) override {
            if (!traits_type::eq_int_type(ch, traits_type::eof())) {
                bytes_.fetch_add(1, std::memory_order_relaxed);
            }
            return traits_type::not_eof(ch);
        }

    private:
        std::atomic<std::uint64_t> records_{0};
        std::atomic<std::uint64_t> bytes_{0};
    };

    // -------- Level / prefix schedules --------

    /// Exactly round(skip * TABLE_SIZE) skipped entries, shuffled with a fixed seed so
    /// both sides walk an identical sequence. Built once, outside any timed region.
    [[nodiscard]] LevelTable make_shuffled(const double skip) {
        LevelTable table{};
        const auto skipped = static_cast<std::size_t>(std::lround(skip * static_cast<double>(TABLE_SIZE)));
        for (std::size_t i = 0; i < TABLE_SIZE; ++i) {
            table[i] = i < skipped ? SKIPPED : PASSING;
        }
        // Constant seed on purpose: both branches must walk a byte-identical sequence,
        // otherwise the two sides are not measuring the same work.
        std::mt19937 rng{0xC0FFEEu};  // NOLINT(cert-msc51-cpp)
        std::ranges::shuffle(table, rng);
        return table;
    }

    /// Half skipped, half passing, in two contiguous runs of 4096. Same 50% skip rate as
    /// the shuffled table, but arranged so whole consumer batches can be skippable.
    [[nodiscard]] LevelTable make_blocked() {
        LevelTable table{};
        for (std::size_t i = 0; i < TABLE_SIZE; ++i) {
            table[i] = i < TABLE_SIZE / 2 ? SKIPPED : PASSING;
        }
        return table;
    }

    /// Records the sink should see across warmup plus the timed loop. Computed exactly
    /// from the table rather than from the nominal skip rate, because the iteration count
    /// is not a whole number of table cycles.
    [[nodiscard]] std::uint64_t
    passes_in(const LevelTable& table, const std::size_t iterations) {
        const std::size_t cycles = iterations / TABLE_SIZE;
        const std::size_t rest   = iterations % TABLE_SIZE;

        std::uint64_t per_cycle = 0;
        for (const LogLevel level : table) {
            per_cycle += level >= THRESHOLD ? 1 : 0;
        }
        std::uint64_t partial = 0;
        for (std::size_t i = 0; i < rest; ++i) {
            partial += table[i] >= THRESHOLD ? 1 : 0;
        }
        return static_cast<std::uint64_t>(cycles) * per_cycle + partial;
    }

    [[nodiscard]] std::uint64_t expected_records(const LevelTable& table,
                                                 const std::size_t threads,
                                                 const std::size_t warmup,
                                                 const std::size_t iterations) {
        return static_cast<std::uint64_t>(threads) * (passes_in(table, warmup) + passes_in(table, iterations));
    }

    // -------- Logger / sink construction --------

    /// pool_size is pinned explicitly: its default differs between the two branches, and
    /// leaving it implicit would fold that change into every other measurement.
    [[nodiscard]] LoggerConfig logger_config(const LoggerConfig::WaitStrategy wait) {
        return LoggerConfig::Builder{}
            .wait_strategy(wait)
            .ring_buffer_size(LoggerConfig::BufferCapacity::Medium)
            .pool_size(2)
            .finalize();
    }

    /// Everything a scenario owns for one repetition. Movable so setup() can return it.
    struct Rig {
        std::unique_ptr<CountingNullBuf> buf;
        std::unique_ptr<std::ostream> out;
        std::unique_ptr<Logger> logger;

        [[nodiscard]] std::uint64_t records() const noexcept {
            return buf ? buf->records() : 0;
        }
    };

    [[nodiscard]] Rig make_console_rig(const LoggerConfig::WaitStrategy wait,
                                       const LogLevel threshold,
                                       PrefixFilter filter = {}) {
        Rig rig{.buf = std::make_unique<CountingNullBuf>(), .out = nullptr, .logger = nullptr};
        rig.out    = std::make_unique<std::ostream>(rig.buf.get());
        rig.logger = std::make_unique<Logger>(logger_config(wait));
        rig.logger->add_sink(std::make_shared<ConsoleSink<LightEntry>>(ConsoleSinkConfig::Builder{}
                                                                          .threshold(threshold)
                                                                          .output(rig.out.get())
                                                                          .enable_colors(false)
                                                                          .flush_each_entry(false)
                                                                          .prefix_filter(std::move(filter))
                                                                          .finalize()));
        return rig;
    }

    [[nodiscard]] Rig make_bare_rig(const LoggerConfig::WaitStrategy wait) {
        return Rig{.buf = nullptr, .out = nullptr, .logger = std::make_unique<Logger>(logger_config(wait))};
    }

    [[nodiscard]] Rig make_file_rig(const LoggerConfig::WaitStrategy wait, const std::filesystem::path& path) {
        std::error_code ec;
        std::filesystem::remove(path, ec);

        Rig rig{.buf = nullptr, .out = nullptr, .logger = std::make_unique<Logger>(logger_config(wait))};
        rig.logger->add_sink(std::make_shared<FileSink<DetailedEntry>>(
            FileSinkConfig::Builder{}
                .threshold(THRESHOLD)
                .file(path)
                .add_time_to_filename(false)
                .rotate_file(false)
                .max_file_size(menagerie::beavers::literals::operator""_mb(4096))
                .flush_each_entry(false)
                .finalize()));
        return rig;
    }

    const std::string PADDING(PAD_SIZE, '0');

    [[nodiscard]] std::string wait_name(const LoggerConfig::WaitStrategy wait) {
        return wait == LoggerConfig::WaitStrategy::BusySpin ? "BusySpin" : "Yielding";
    }

    [[nodiscard]] std::string rate_str(const double value) {
        return std::format("{:.2f}", value);
    }

    // -------- Scenarios --------

    /// S1/S2: the headline sweep. Shuffled defeats the per-batch skip (nearly every batch
    /// holds a passing event) so it measures the frontend gate alone; blocked lets the
    /// per-batch skip fire, and the difference between them is that optimization's value.
    void level_sweep(const std::string& variant,
                     const LevelTable& table,
                     const double skip,
                     const std::size_t threads,
                     const LoggerConfig::WaitStrategy wait) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;

        bench::ScenarioSpec spec{
            .name           = "level_sweep",
            .variant        = variant,
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"skip_rate", rate_str(skip)},
                               {"sink", "console_null"},
                               {"entry", "light"},
                               {"wait", wait_name(wait)},
                               {"threshold", "WRN"},
                               {"payload_bytes", std::to_string(PAD_SIZE)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_console_rig(wait, THRESHOLD); },
            [&](Rig& rig, std::size_t, const std::size_t j) {
                rig.logger->log(table[j & TABLE_MASK],
                                std::string_view{},
                                std::source_location::current(),
                                "{}",
                                PADDING);
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        bench::verify_emitted(result, expected_records(table, threads, warmup, iterations));
        bench::report(result);
    }

    /// S3: the apples-to-apples control. The caller does the formatting itself and passes
    /// a string_view, so formatting work is identical on both sides at every skip rate.
    /// Whatever S1 gains beyond this row is the value of skipped formatting specifically.
    void fixed_work_sweep(const LevelTable& table, const double skip, const std::size_t threads) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;
        constexpr auto wait          = LoggerConfig::WaitStrategy::Yielding;

        bench::ScenarioSpec spec{
            .name           = "fixed_work",
            .variant        = "console_null/shuffled",
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"skip_rate", rate_str(skip)},
                               {"sink", "console_null"},
                               {"entry", "light"},
                               {"wait", wait_name(wait)},
                               {"threshold", "WRN"},
                               {"payload_bytes", std::to_string(PAD_SIZE)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_console_rig(wait, THRESHOLD); },
            [&](Rig& rig, std::size_t, const std::size_t j) {
                thread_local std::string buffer;
                buffer.clear();
                std::format_to(std::back_inserter(buffer), "{}", PADDING);
                rig.logger->log(table[j & TABLE_MASK], std::string_view{}, buffer, std::source_location::current());
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        bench::verify_emitted(result, expected_records(table, threads, warmup, iterations));
        bench::report(result);
    }

    /// S4: prefix rejection. The level gate cannot fire here (threshold is Trace), so
    /// this measures what the gate does NOT cover. A near-zero delta is the expected and
    /// informative result: it sizes what a prefix-aware gate would be worth.
    void prefix_sweep(const double drop_rate, const std::size_t threads) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;
        constexpr auto wait          = LoggerConfig::WaitStrategy::Yielding;

        // "keep" and "drop" are the same length and both non-empty: PrefixFilter accepts
        // an empty prefix regardless of mode, and LightEntry ignores the prefix entirely,
        // so the emitted record is byte-identical either way.
        std::array<bool, TABLE_SIZE> keep{};
        const auto dropped = static_cast<std::size_t>(std::lround(drop_rate * static_cast<double>(TABLE_SIZE)));
        for (std::size_t i = 0; i < TABLE_SIZE; ++i) {
            keep[i] = i >= dropped;
        }
        // Constant seed on purpose: both branches must walk a byte-identical sequence,
        // otherwise the two sides are not measuring the same work.
        std::mt19937 rng{0xC0FFEEu};  // NOLINT(cert-msc51-cpp)
        std::ranges::shuffle(keep, rng);

        std::uint64_t per_cycle = 0;
        for (const bool k : keep) {
            per_cycle += k ? 1 : 0;
        }
        const auto passes = [&](const std::size_t count) {
            std::uint64_t partial = 0;
            for (std::size_t i = 0; i < count % TABLE_SIZE; ++i) {
                partial += keep[i] ? 1 : 0;
            }
            return static_cast<std::uint64_t>(count / TABLE_SIZE) * per_cycle + partial;
        };

        bench::ScenarioSpec spec{
            .name           = "prefix_sweep",
            .variant        = "console_null",
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"drop_rate", rate_str(drop_rate)},
                               {"sink", "console_null"},
                               {"entry", "light"},
                               {"wait", wait_name(wait)},
                               {"threshold", "TRC"},
                               {"payload_bytes", std::to_string(PAD_SIZE)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] {
                return make_console_rig(wait,
                                        LogLevel::Trace,
                                        PrefixFilter::allow(PrefixFilter::Set{"keep"}));
            },
            [&](Rig& rig, std::size_t, const std::size_t j) {
                rig.logger->log(PASSING,
                                keep[j & TABLE_MASK] ? std::string_view{"keep"} : std::string_view{"drop"},
                                std::source_location::current(),
                                "{}",
                                PADDING);
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        bench::verify_emitted(result,
                              static_cast<std::uint64_t>(threads) * (passes(warmup) + passes(iterations)));
        bench::report(result);
    }

    /// S5: the stream() path. A gated proxy still constructs its ostringstream and assigns
    /// the prefix; only the publish is skipped. Sizes the case for gating earlier.
    void stream_sweep(const LevelTable& table, const double skip, const std::size_t threads) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;
        constexpr auto wait          = LoggerConfig::WaitStrategy::Yielding;

        bench::ScenarioSpec spec{
            .name           = "stream_sweep",
            .variant        = "console_null",
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"skip_rate", rate_str(skip)},
                               {"sink", "console_null"},
                               {"entry", "light"},
                               {"wait", wait_name(wait)},
                               {"threshold", "WRN"},
                               {"payload_bytes", std::to_string(PAD_SIZE)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_console_rig(wait, THRESHOLD); },
            [&](Rig& rig, std::size_t, const std::size_t j) {
                rig.logger->stream(table[j & TABLE_MASK], std::string_view{}) << PADDING;
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        bench::verify_emitted(result, expected_records(table, threads, warmup, iterations));
        bench::report(result);
    }

    /// S6: no sinks at all -- a degenerate 100% skip, and the newer branch's ceiling.
    /// If this row is not the largest win of the run, the gate is not firing and every
    /// other number in the run is suspect.
    void no_sinks(const std::size_t threads) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;
        constexpr auto wait          = LoggerConfig::WaitStrategy::Yielding;

        bench::ScenarioSpec spec{
            .name           = "no_sinks",
            .variant        = "",
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"skip_rate", "1.00"}, {"sink", "none"}, {"wait", wait_name(wait)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_bare_rig(wait); },
            [&](Rig& rig, std::size_t, std::size_t) {
                rig.logger->log(PASSING, std::string_view{}, std::source_location::current(), "{}", PADDING);
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        bench::verify_emitted(result, 0);
        bench::report(result);
    }

    /// S7/S8: a real FileSink writing to disk. Confounded on purpose and labelled as such
    /// -- FileSink's internals were reworked alongside the gate, so a delta here is
    /// "gating plus sink rework", not gating. Kept because it is what a user actually runs.
    void file_stack(const double skip, const bool timed_teardown, const std::string& out_dir) {
        const LevelTable table = make_shuffled(skip);
        constexpr auto wait    = LoggerConfig::WaitStrategy::Yielding;
        const std::filesystem::path path =
            std::filesystem::path{out_dir} / (timed_teardown ? "crow_gate_e2e.log" : "crow_gate_stack.log");

        bench::ScenarioSpec spec{
            .name           = "filesink_whole_stack",
            .variant        = timed_teardown ? "e2e" : "enqueue",
            .threads        = bench::CONTENTION_THREADS,
            .iterations     = ITER_FILE,
            .warmup         = timed_teardown ? 0 : WARMUP_FILE,
            .timed_teardown = timed_teardown,
            .params         = {{"skip_rate", rate_str(skip)},
                               {"sink", "file"},
                               {"entry", "detailed"},
                               {"wait", wait_name(wait)},
                               {"threshold", "WRN"},
                               {"confounded", "gating+sink_rework"}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [&] { return make_file_rig(wait, path); },
            [&](Rig& rig, std::size_t, const std::size_t j) {
                rig.logger->log(table[j & TABLE_MASK],
                                std::string_view{},
                                std::source_location::current(),
                                "{}",
                                PADDING);
            },
            [](Rig& rig) {
                rig.logger->shutdown();
                return rig.records();
            });

        // The file sink does not report a count, so this row is reported without the
        // emitted-record check the console rows carry.
        bench::report(result);

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    /// C1: no logger at all. Identical source on both sides, so whatever delta this shows
    /// is machine drift plus code layout -- the noise floor every other row is judged against.
    void format_only(const std::size_t threads) {
        const std::size_t iterations = threads == 1 ? ITER_1T : ITER_8T;
        const std::size_t warmup     = threads == 1 ? WARMUP_1T : WARMUP_8T;

        bench::ScenarioSpec spec{
            .name           = "control",
            .variant        = "format_only",
            .threads        = threads,
            .iterations     = iterations,
            .warmup         = warmup,
            .timed_teardown = false,
            .params         = {{"sink", "none"}, {"payload_bytes", std::to_string(PAD_SIZE)}},
        };
        if (!bench::selected(spec)) {
            return;
        }

        auto result = bench::run_scenario(
            spec,
            [] { return 0; },
            [&](int&, std::size_t, std::size_t) {
                thread_local std::string buffer;
                buffer.clear();
                std::format_to(std::back_inserter(buffer), "{}", PADDING);
                // Keep the formatted bytes observable so the whole call cannot be elided.
                bench::detail::sink_value(buffer.size());
            },
            [](int&) { return std::uint64_t{0}; });

        bench::verify_emitted(result, 0);
        bench::report(result);
    }

}  // namespace

int main(const int argc, char** argv) {
    bench::parse_args(argc, argv);
    bench::env_check();

    const std::string out_dir = bench::run_context().out_dir;

    std::cout << "\ncrow gate benchmark -- skip-rate sweep\n"
              << "  threads: " << bench::CONTENTION_THREADS << " (contended) / " << bench::BASELINE_THREADS
              << " (baseline)\n"
              << "  reps:    " << bench::run_context().reps << "\n";

    // Noise floor first, so it is on record even if a later scenario aborts.
    format_only(bench::CONTENTION_THREADS);
    format_only(bench::BASELINE_THREADS);

    // S1 -- the headline sweep, full resolution at 8 threads on the library default
    // wait strategy; the ends only elsewhere, to keep the run under ten minutes a side.
    for (const double skip : {0.00, 0.25, 0.50, 0.75, 1.00}) {
        level_sweep("console_null/shuffled",
                    make_shuffled(skip),
                    skip,
                    bench::CONTENTION_THREADS,
                    LoggerConfig::WaitStrategy::Yielding);
    }
    for (const double skip : {0.00, 1.00}) {
        level_sweep("console_null/shuffled",
                    make_shuffled(skip),
                    skip,
                    bench::BASELINE_THREADS,
                    LoggerConfig::WaitStrategy::Yielding);
        level_sweep("console_null/shuffled",
                    make_shuffled(skip),
                    skip,
                    bench::CONTENTION_THREADS,
                    LoggerConfig::WaitStrategy::BusySpin);
    }

    // S2 -- same 50% skip, arranged so whole batches are skippable.
    level_sweep("console_null/blocked",
                make_blocked(),
                0.50,
                bench::CONTENTION_THREADS,
                LoggerConfig::WaitStrategy::Yielding);

    // S3 -- formatting held constant across skip rates.
    for (const double skip : {0.00, 0.50, 1.00}) {
        fixed_work_sweep(make_shuffled(skip), skip, bench::CONTENTION_THREADS);
    }

    // S4 -- prefix rejection, which the level gate does not cover.
    for (const double drop : {0.00, 0.50, 1.00}) {
        prefix_sweep(drop, bench::CONTENTION_THREADS);
    }

    // S5 -- the stream() path.
    for (const double skip : {0.00, 1.00}) {
        stream_sweep(make_shuffled(skip), skip, bench::CONTENTION_THREADS);
    }

    // S6 -- the ceiling.
    no_sinks(bench::CONTENTION_THREADS);
    no_sinks(bench::BASELINE_THREADS);

    // S7/S8 -- whole stack on real files.
    for (const double skip : {0.00, 1.00}) {
        file_stack(skip, false, out_dir);
        file_stack(skip, true, out_dir);
    }

    std::cout << "\ndone\n";
    return 0;
}
