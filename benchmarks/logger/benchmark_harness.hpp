#pragma once

#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <menagerie/chameleon>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Shared harness for the logger benchmarks.
 *
 * Two APIs live here. `run_scenario()` is the current one: warmup outside the clock,
 * repetitions with a median, a start barrier that actually synchronizes thread starts,
 * and one JSON line per repetition for machine comparison. `run_threaded_benchmark()`
 * and its e2e variant are the older shape, kept because two existing benchmarks use
 * them; they share the fixed barrier but do no warmup or repetition.
 */
namespace bench {

    constexpr std::size_t CONTENTION_THREADS    = 8;
    constexpr std::size_t BASELINE_THREADS      = 1;
    constexpr std::size_t ITERATIONS_PER_THREAD = 1'000'000;
    constexpr std::size_t TARGET_RECORD_SIZE    = 256;

    /// Warmup must exceed the ring capacity so every slot's message string reaches
    /// steady-state capacity and the format path stops allocating mid-measurement.
    constexpr std::size_t MIN_WARMUP_ITERATIONS = 8192;

    // -------- Run-level context --------

    /// Set once from argv; describes the run rather than any single scenario.
    struct RunContext {
        std::string binary;                                        ///< Binary name, for the JSON.
        std::string out_dir  = ".";                                ///< Where scenarios put payload files.
        std::string filter;                                        ///< Substring; empty runs everything.
        std::size_t reps     = 3;                                  ///< Measured repetitions per scenario.
        std::ofstream json_file;                                   ///< Optional JSON sink.
        std::vector<std::pair<std::string, std::string>> meta;     ///< Provenance from the runner.
    };

    inline RunContext& run_context() {
        static RunContext ctx;
        return ctx;
    }

    /// Reads a single-line sysfs value, or nullopt when the file is absent.
    [[nodiscard]] inline std::optional<std::string> read_sysfs(const std::string& path) {
        std::ifstream in{path};
        if (!in) {
            return std::nullopt;
        }
        std::string value;
        std::getline(in, value);
        return value;
    }

    /**
     * @brief Warns about machine state that makes timings untrustworthy. Never fails.
     *
     * Modelled on env_check() in the resource_pool benchmarks. The values also go into
     * the JSON so an aggregator can refuse to compare two runs made under different
     * governors.
     */
    inline void env_check() {
        if (const auto gov = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
            gov && *gov != "performance") {
            std::cerr << "[warn] CPU governor is '" << *gov << "', not 'performance'.\n"
                      << "       sudo cpupower frequency-set -g performance\n";
        }
        if (const auto smt = read_sysfs("/sys/devices/system/cpu/smt/active"); smt && *smt == "1") {
            std::cerr << "[warn] SMT is active; sibling contention will show up in 8-thread rows.\n";
        }
        if (const auto load = read_sysfs("/proc/loadavg"); load && !load->empty()) {
            if (const double one_min = std::strtod(load->c_str(), nullptr); one_min > 2.0) {
                std::cerr << "[warn] 1-minute load average is " << one_min << "; the box is busy.\n";
            }
        }
    }

    [[nodiscard]] inline std::string governor() {
        return read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor").value_or("unknown");
    }

    [[nodiscard]] inline std::string smt_active() {
        return read_sysfs("/sys/devices/system/cpu/smt/active").value_or("unknown");
    }

    /// Parses --json/--out-dir/--reps/--scenario/--meta. Unknown arguments are ignored.
    inline void parse_args(const int argc, char** argv) {
        RunContext& ctx = run_context();
        ctx.binary      = argc > 0 ? argv[0] : "unknown";

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            const auto next = [&]() -> std::string {
                return i + 1 < argc ? std::string{argv[++i]} : std::string{};
            };

            if (arg == "--json") {
                if (const std::string path = next(); !path.empty()) {
                    ctx.json_file.open(path, std::ios::app);
                    if (!ctx.json_file) {
                        std::cerr << "[warn] cannot open JSON output '" << path << "'; continuing without it.\n";
                    }
                }
            } else if (arg == "--out-dir") {
                ctx.out_dir = next();
            } else if (arg == "--reps") {
                if (const std::string value = next(); !value.empty()) {
                    ctx.reps = std::max<std::size_t>(1, static_cast<std::size_t>(std::stoul(value)));
                }
            } else if (arg == "--scenario") {
                ctx.filter = next();
            } else if (arg == "--meta") {
                const std::string kv = next();
                if (const auto eq = kv.find('='); eq != std::string::npos) {
                    ctx.meta.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
                }
            }
        }
    }

    // -------- Scenario description and results --------

    struct ScenarioSpec {
        std::string name;                                            ///< e.g. "level_sweep".
        std::string variant;                                         ///< e.g. "console_null/shuffled".
        std::size_t threads    = 1;
        std::size_t iterations = 0;                                  ///< Timed iterations per thread.
        std::size_t warmup     = 0;                                  ///< Per thread; 0 selects 5%.
        bool timed_teardown    = false;                              ///< Include teardown in the clock (e2e).
        std::vector<std::pair<std::string, std::string>> params;     ///< skip_rate, sink, wait, ...

        [[nodiscard]] std::string full_name() const {
            return variant.empty() ? name : std::format("{}/{}", name, variant);
        }
    };

    struct RepResult {
        std::chrono::nanoseconds wall{};
        std::vector<std::chrono::nanoseconds> thread_times;
        std::uint64_t emitted          = 0;   ///< Records the sink actually saw.
        std::uint64_t emitted_expected = 0;   ///< What the scenario predicted.
        bool ok                        = true;
    };

    struct ScenarioResult {
        ScenarioSpec spec;
        std::vector<RepResult> reps;

        [[nodiscard]] std::size_t attempted() const {
            return spec.threads * spec.iterations;
        }

        /// Median wall clock across repetitions. Median, never mean: one descheduled
        /// repetition should not move the reported number.
        [[nodiscard]] std::chrono::nanoseconds median_wall() const {
            if (reps.empty()) {
                return {};
            }
            std::vector<std::chrono::nanoseconds> sorted;
            sorted.reserve(reps.size());
            for (const auto& rep : reps) {
                sorted.push_back(rep.wall);
            }
            std::ranges::sort(sorted);
            return sorted[sorted.size() / 2];
        }

        /// (max - min) / median across repetitions; a spread above ~5% means the box moved.
        [[nodiscard]] double rel_spread() const {
            if (reps.size() < 2) {
                return 0.0;
            }
            auto [lo, hi] = std::ranges::minmax_element(reps, {}, &RepResult::wall);
            const auto med = median_wall().count();
            if (med == 0) {
                return 0.0;
            }
            return static_cast<double>(hi->wall.count() - lo->wall.count()) / static_cast<double>(med);
        }

        /// Attempted log() calls per second: the metric the calling application experiences.
        /// Deliberately NOT per emitted record -- at a high skip rate the two sides emit
        /// nothing while still attempting the same calls.
        [[nodiscard]] double attempted_per_sec() const {
            const auto wall = median_wall().count();
            return wall == 0 ? 0.0 : static_cast<double>(attempted()) * 1e9 / static_cast<double>(wall);
        }

        [[nodiscard]] bool all_ok() const {
            return std::ranges::all_of(reps, &RepResult::ok);
        }
    };

    // -------- The runner --------

    namespace detail {
        /// Keeps a computed value observable so the optimizer cannot elide the work that
        /// produced it. Needed by control scenarios whose result is otherwise unused.
        inline void sink_value(const std::size_t value) {
            asm volatile("" : : "r"(value) : "memory");  // NOLINT(hicpp-no-assembler)
        }

        template <typename Handle, typename LogFn>
        void run_threads(const std::size_t threads,
                         const std::size_t warmup,
                         const std::size_t iterations,
                         Handle& handle,
                         LogFn& log_fn,
                         std::vector<std::chrono::nanoseconds>& thread_times,
                         std::chrono::steady_clock::time_point& wall_start) {
            // +1 for the timing thread, so the wall clock starts only once every worker
            // has finished warming up and is about to enter its timed loop.
            std::barrier start_gate{static_cast<std::ptrdiff_t>(threads + 1)};
            std::vector<std::thread> workers;
            workers.reserve(threads);

            for (std::size_t i = 0; i < threads; ++i) {
                workers.emplace_back([&, id = i] {
                    for (std::size_t j = 0; j < warmup; ++j) {
                        log_fn(handle, id, j);
                    }
                    start_gate.arrive_and_wait();

                    const auto t0 = std::chrono::steady_clock::now();
                    for (std::size_t j = 0; j < iterations; ++j) {
                        log_fn(handle, id, j);
                    }
                    thread_times[id] = std::chrono::steady_clock::now() - t0;
                });
            }

            start_gate.arrive_and_wait();
            wall_start = std::chrono::steady_clock::now();

            for (auto& worker : workers) {
                worker.join();
            }
        }
    }  // namespace detail

    /**
     * @brief Runs one scenario: warmup, N repetitions, median reported.
     *
     * @param setup     Called fresh per repetition; returns a handle owning the Logger and sinks.
     *                  A Logger cannot be reused across repetitions -- shutdown() is terminal.
     * @param log_fn    log_fn(handle, thread_id, iteration); the call under measurement.
     * @param teardown  teardown(handle) -> emitted record count. Inside the clock when
     *                  spec.timed_teardown is set.
     *
     * Scenarios with a timed teardown discard a whole first repetition instead of doing an
     * in-loop warmup: warmup events would still be in the ring when the barrier releases,
     * so the consumer would drain them inside the timed region.
     */
    template <typename SetupFn, typename LogFn, typename TeardownFn>
    ScenarioResult run_scenario(ScenarioSpec spec, SetupFn&& setup, LogFn&& log_fn, TeardownFn&& teardown) {
        const RunContext& ctx = run_context();

        if (spec.warmup == 0 && !spec.timed_teardown) {
            spec.warmup = std::max(MIN_WARMUP_ITERATIONS, spec.iterations / 20);
        }
        if (!spec.timed_teardown && spec.warmup < MIN_WARMUP_ITERATIONS) {
            std::cerr << "[warn] " << spec.full_name() << ": warmup " << spec.warmup << " is below the ring size; "
                      << "the format path may still be allocating when the clock starts.\n";
        }

        ScenarioResult result{.spec = spec, .reps = {}};

        // A discarded repetition stands in for in-loop warmup when teardown is timed.
        const std::size_t total_reps = ctx.reps + (spec.timed_teardown ? 1 : 0);

        for (std::size_t rep = 0; rep < total_reps; ++rep) {
            auto handle = setup();
            std::vector<std::chrono::nanoseconds> thread_times(spec.threads);
            std::chrono::steady_clock::time_point wall_start{};

            detail::run_threads(spec.threads,
                                spec.timed_teardown ? 0 : spec.warmup,
                                spec.iterations,
                                handle,
                                log_fn,
                                thread_times,
                                wall_start);

            std::uint64_t emitted = 0;
            std::chrono::steady_clock::time_point wall_end{};
            if (spec.timed_teardown) {
                emitted  = teardown(handle);
                wall_end = std::chrono::steady_clock::now();
            } else {
                wall_end = std::chrono::steady_clock::now();
                emitted  = teardown(handle);
            }

            if (spec.timed_teardown && rep == 0) {
                continue;  // discarded warmup repetition
            }

            result.reps.push_back(RepResult{
                .wall             = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start),
                .thread_times     = std::move(thread_times),
                .emitted          = emitted,
                .emitted_expected = 0,
                .ok               = true,
            });
        }

        return result;
    }

    /**
     * @brief Marks every repetition against the record count the scenario predicted.
     *
     * A scenario whose sink saw the wrong number of records is not reportable: the most
     * dangerous failure mode here is a sink dying, the gate opening, and the resulting
     * "no work done" being read as a speedup.
     */
    inline void verify_emitted(ScenarioResult& result, const std::uint64_t expected) {
        for (auto& rep : result.reps) {
            rep.emitted_expected = expected;
            rep.ok               = rep.emitted == expected;
            if (!rep.ok) {
                std::cerr << "[FAIL] " << result.spec.full_name() << ": sink saw " << rep.emitted << " records, expected "
                          << expected << " -- scenario not reportable.\n";
            }
        }
    }

    // -------- Output --------

    inline void print_results(const ScenarioResult& result) {
        if (result.reps.empty()) {
            std::cerr << "[warn] " << result.spec.full_name() << ": no repetitions recorded.\n";
            return;
        }

        const double wall_sec = static_cast<double>(result.median_wall().count()) / 1e9;
        const double per_sec  = result.attempted_per_sec();
        const double ns_per   = per_sec == 0.0 ? 0.0 : 1e9 / per_sec;

        auto section = menagerie::chameleon::section("")
                           .row("Threads", result.spec.threads)
                           .row("Iterations/thread", result.spec.iterations)
                           .row("Repetitions", result.reps.size());
        for (const auto& [key, value] : result.spec.params) {
            section = section.row(key, value);
        }

        const std::string body = section.row("Median wall", std::format("{:.3f} s", wall_sec))
                                     .row("Attempted/s", std::format("{:.0f}", per_sec))
                                     .row("ns/attempt", std::format("{:.1f}", ns_per))
                                     .row("Emitted", result.reps.front().emitted)
                                     .row("Spread", std::format("{:.1f}%", result.rel_spread() * 100.0))
                                     .row("Status", result.all_ok() ? "ok" : "FAILED VERIFICATION")
                                     .indent_size(1)
                                     .value_align(menagerie::chameleon::Align::Right)
                                     .render();

        std::cout << '\n'
                  << menagerie::chameleon::box(body)
                         .title(result.spec.full_name())
                         .border(menagerie::chameleon::border::unicode)
                         .border_style(result.all_ok() ? menagerie::chameleon::colors::bold_cyan
                                                       : menagerie::chameleon::colors::bold_red)
                         .padding(1)
                         .terminate()
                         .render();
    }

    namespace detail {
        [[nodiscard]] inline std::string json_escape(const std::string_view text) {
            std::string out;
            out.reserve(text.size() + 8);
            for (const char ch : text) {
                switch (ch) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    default:
                        out += ch;
                }
            }
            return out;
        }
    }  // namespace detail

    /// Emits one JSON line per repetition. Aggregation (median, joins, deltas) is the
    /// runner's job -- keeping it out of C++ means a re-analysis needs no rebuild.
    inline void emit_json(const ScenarioResult& result) {
        RunContext& ctx = run_context();
        if (!ctx.json_file.is_open()) {
            return;
        }

        for (std::size_t rep = 0; rep < result.reps.size(); ++rep) {
            const RepResult& r = result.reps[rep];

            const auto wall_ns    = static_cast<double>(r.wall.count());
            const auto attempted  = static_cast<double>(result.attempted());
            const double att_per_sec = wall_ns == 0.0 ? 0.0 : attempted * 1e9 / wall_ns;
            const double emit_per_sec =
                wall_ns == 0.0 ? 0.0 : static_cast<double>(r.emitted) * 1e9 / wall_ns;

            auto [lo, hi] = std::ranges::minmax_element(r.thread_times);
            const auto sum =
                std::accumulate(r.thread_times.begin(), r.thread_times.end(), std::chrono::nanoseconds{});
            const auto mean = r.thread_times.empty()
                                  ? std::chrono::nanoseconds{}
                                  : sum / static_cast<std::int64_t>(r.thread_times.size());

            std::string line = "{";
            line += R"("schema":1)";
            for (const auto& [key, value] : ctx.meta) {
                line += std::format(R"(,"{}":"{}")", detail::json_escape(key), detail::json_escape(value));
            }
            line += std::format(R"(,"binary":"{}")", detail::json_escape(ctx.binary));
            line += std::format(R"(,"scenario":"{}","variant":"{}")",
                                detail::json_escape(result.spec.name),
                                detail::json_escape(result.spec.variant));
            line += std::format(R"(,"rep":{},"threads":{},"iterations":{},"warmup":{})",
                                rep + 1,
                                result.spec.threads,
                                result.spec.iterations,
                                result.spec.warmup);

            line += R"(,"params":{)";
            bool first = true;
            for (const auto& [key, value] : result.spec.params) {
                line += std::format(R"({}"{}":"{}")",
                                    first ? "" : ",",
                                    detail::json_escape(key),
                                    detail::json_escape(value));
                first = false;
            }
            line += "}";

            line += std::format(R"(,"wall_ns":{},"attempted":{},"emitted":{},"emitted_expected":{})",
                                r.wall.count(),
                                result.attempted(),
                                r.emitted,
                                r.emitted_expected);
            line += std::format(R"(,"attempted_per_sec":{:.1f},"emitted_per_sec":{:.1f},"ns_per_attempt":{:.3f})",
                                att_per_sec,
                                emit_per_sec,
                                att_per_sec == 0.0 ? 0.0 : 1e9 / att_per_sec);
            line += std::format(R"(,"thread_ns":{{"min":{},"max":{},"mean":{}}})",
                                r.thread_times.empty() ? 0 : lo->count(),
                                r.thread_times.empty() ? 0 : hi->count(),
                                mean.count());
            line += std::format(R"(,"governor":"{}","smt":"{}","ok":{})",
                                detail::json_escape(governor()),
                                detail::json_escape(smt_active()),
                                r.ok ? "true" : "false");
            line += "}";

            ctx.json_file << line << '\n';
        }
        ctx.json_file.flush();
    }

    /// True when the scenario passes the --scenario substring filter.
    [[nodiscard]] inline bool selected(const ScenarioSpec& spec) {
        const std::string& filter = run_context().filter;
        return filter.empty() || spec.full_name().find(filter) != std::string::npos;
    }

    /// Prints and records a finished scenario.
    inline void report(const ScenarioResult& result) {
        print_results(result);
        emit_json(result);
    }

    // -------- Legacy API, used by the file/abseil benchmarks --------

    struct ThreadResult {
        std::chrono::nanoseconds completion_time{};
    };

    struct BenchmarkResult {
        std::size_t thread_count                 = 0;
        std::size_t iterations                   = 0;
        std::vector<ThreadResult> thread_results = {};
        std::chrono::nanoseconds wall_clock      = {};
        double entries_per_second                = 0.0;
    };

    inline void
    print_results(const BenchmarkResult& result, const std::string_view prefix, const std::string_view title) {
        if (result.thread_results.empty() || result.wall_clock.count() == 0) {
            std::cerr << "[warn] " << prefix << ": " << title << " produced no usable timing.\n";
            return;
        }

        const double wall_sec = static_cast<double>(result.wall_clock.count()) / 1e9;

        std::chrono::nanoseconds min_time = result.thread_results.front().completion_time;
        std::chrono::nanoseconds max_time = result.thread_results.front().completion_time;
        std::chrono::nanoseconds sum_time{};

        for (const auto& tr : result.thread_results) {
            min_time  = std::min(min_time, tr.completion_time);
            max_time  = std::max(max_time, tr.completion_time);
            sum_time += tr.completion_time;
        }

        const double avg_sec =
            static_cast<double>(sum_time.count()) / static_cast<double>(result.thread_results.size()) / 1e9;
        const double min_sec = static_cast<double>(min_time.count()) / 1e9;
        const double max_sec = static_cast<double>(max_time.count()) / 1e9;

        const std::string body = menagerie::chameleon::section("")
                                     .row("Threads", result.thread_count)
                                     .row("Iterations/thread", result.iterations)
                                     .row("Target record", "~256 bytes")
                                     .row("Wall-clock", std::format("{:.3f} s", wall_sec))
                                     .row("Throughput", std::format("{:.0f} ops/s", result.entries_per_second))
                                     .row("Avg thread time", std::format("{:.3f} s", avg_sec))
                                     .row("Min thread time", std::format("{:.3f} s", min_sec))
                                     .row("Max thread time", std::format("{:.3f} s", max_sec))
                                     .indent_size(1)
                                     .value_align(menagerie::chameleon::Align::Right)
                                     .render();

        std::cout << '\n'
                  << menagerie::chameleon::box(body)
                         .title(std::format("{}: {}", prefix, title))
                         .border(menagerie::chameleon::border::unicode)
                         .border_style(menagerie::chameleon::colors::bold_cyan)
                         .padding(1)
                         .terminate()
                         .render();
    }

    namespace detail {
        template <typename LogFn>
        BenchmarkResult run_legacy(const std::size_t thread_count,
                                   const std::size_t iterations,
                                   LogFn& log_fn,
                                   const bool timed_teardown,
                                   auto&& teardown_fn) {
            std::vector<ThreadResult> thread_results(thread_count);
            std::vector<std::chrono::nanoseconds> thread_times(thread_count);
            std::chrono::steady_clock::time_point wall_start{};

            int unused_handle = 0;
            auto adapter      = [&](int&, std::size_t, std::size_t) { log_fn(); };
            run_threads(thread_count, 0, iterations, unused_handle, adapter, thread_times, wall_start);

            if (timed_teardown) {
                teardown_fn();
            }
            const auto wall_end = std::chrono::steady_clock::now();

            for (std::size_t i = 0; i < thread_count; ++i) {
                thread_results[i].completion_time = thread_times[i];
            }

            const auto wall_clock    = wall_end - wall_start;
            const double wall_sec    = std::chrono::duration<double>(wall_clock).count();
            const auto total_entries = static_cast<double>(thread_count * iterations);

            return BenchmarkResult{
                .thread_count       = thread_count,
                .iterations         = iterations,
                .thread_results     = std::move(thread_results),
                .wall_clock         = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock),
                .entries_per_second = wall_sec == 0.0 ? 0.0 : total_entries / wall_sec,
            };
        }
    }  // namespace detail

    /// Runs log_fn on every thread; the wall clock covers the timed loops only.
    template <typename LogFn>
    BenchmarkResult
    run_threaded_benchmark(const std::size_t thread_count, const std::size_t iterations, LogFn&& log_fn) {
        return detail::run_legacy(thread_count, iterations, log_fn, false, [] {});
    }

    /// As run_threaded_benchmark, but the wall clock also covers shutdown_fn(): producer
    /// work plus async drain plus shutdown, which is the honest number for an async logger.
    template <typename LogFn, typename ShutdownFn>
    BenchmarkResult run_threaded_benchmark_e2e(const std::size_t thread_count,
                                               const std::size_t iterations,
                                               LogFn&& log_fn,
                                               ShutdownFn&& shutdown_fn) {
        return detail::run_legacy(thread_count, iterations, log_fn, true, shutdown_fn);
    }

}  // namespace bench
