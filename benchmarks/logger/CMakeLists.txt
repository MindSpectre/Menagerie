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
