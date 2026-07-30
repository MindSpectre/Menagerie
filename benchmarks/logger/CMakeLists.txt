##############################################################################
# Benchmark Scroll file logger (8-thread contention + 1-thread baseline)
##############################################################################
add_executable(${BENCHMARKS}.Scroll.FileBenchmark
        benchmark_harness.hpp
        crow_file_benchmark.cpp
)
target_link_libraries(${BENCHMARKS}.Scroll.FileBenchmark
        PRIVATE
        Menagerie::Common::Crow
        Menagerie::Common::Spider
        Menagerie::Common::Math
        Menagerie::Common::Chameleon
)

##############################################################################
# Crow gate benchmark: skip-rate sweep, built identically on both branches.
#
# This is the A/B subject. Its source is carried across checkouts (see
# run_gate_bench.sh), so it must only use crow APIs that exist on both sides.
##############################################################################
add_executable(${BENCHMARKS}.Crow.GateBenchmark
        benchmark_harness.hpp
        crow_gate_benchmark.cpp
)
target_link_libraries(${BENCHMARKS}.Crow.GateBenchmark
        PRIVATE
        Menagerie::Common::Crow
        Menagerie::Common::Spider
        Menagerie::Common::Math
        Menagerie::Common::Chameleon
)

##############################################################################
# Crow termination benchmark: measures features that exist only on the
# sink-termination branch (sink health, janitor, registry churn).
#
# Detected by file existence rather than an option(): sink_status.hpp is the
# artifact that actually determines whether this file compiles, so the check
# needs no operator action, survives every checkout, and fails closed on a
# branch without it. The STATUS line is also the operator's proof they built
# the side they think they did.
##############################################################################
if (EXISTS "${PROJECT_SOURCE_DIR}/common/crow/sink/detail/sink_status.hpp")
    message(STATUS "crow: sink-termination API detected — branch-only benchmarks enabled")
    add_executable(${BENCHMARKS}.Crow.TerminationBenchmark
            benchmark_harness.hpp
            crow_termination_benchmark.cpp
    )
    target_link_libraries(${BENCHMARKS}.Crow.TerminationBenchmark
            PRIVATE
            Menagerie::Common::Crow
            Menagerie::Common::Spider
            Menagerie::Common::Math
            Menagerie::Common::Chameleon
    )
else ()
    message(STATUS "crow: sink-termination API absent — branch-only benchmarks skipped (comparable A/B mode)")
endif ()

##############################################################################
# Benchmark Abseil file logger (8-thread contention + 1-thread baseline)
# Only built when the "abseil-benchmarks" vcpkg feature is enabled.
##############################################################################
find_package(absl CONFIG QUIET)

if (absl_FOUND)
    add_executable(${BENCHMARKS}.Abseil.FileBenchmark
            benchmark_harness.hpp
            abseil_file_benchmark.cpp
    )
    target_link_libraries(${BENCHMARKS}.Abseil.FileBenchmark
            PRIVATE
            absl::log
            absl::log_sink
            absl::log_initialize
            absl::log_globals
            Menagerie::Common::Beavers
            Menagerie::Common::Chameleon
    )
else ()
    message(STATUS "Abseil not found — skipping Abseil benchmarks (enable vcpkg feature 'abseil-benchmarks')")
endif ()
