set(BENCHMARKS_ELEPHANT ${BENCHMARKS}.Savanna.Elephant)
##############################################################################
# PostgreSQL Client Overhead Benchmarks
# Requires vcpkg feature 'pg-benchmarks' (Google Benchmark + libpqxx)
##############################################################################
find_package(benchmark CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)

# ─── Shared fixture library ──────────────────────────────────────────
add_library(${BENCHMARKS_ELEPHANT}.Common STATIC
        common/bench_fixture.cpp
)
target_include_directories(${BENCHMARKS_ELEPHANT}.Common PUBLIC common)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Common
        PUBLIC
        benchmark::benchmark
        benchmark::benchmark_main
        PostgreSQL::PostgreSQL
        Boost::headers
        Menagerie::Common::Beaver
)

add_subdirectory(concurrency_scenarios)

##############################################################################
# Benchmark Raw String vs Compiled Query
##############################################################################
add_executable(${BENCHMARKS_ELEPHANT}.RawVsCompiled
        raw_vs_compiled_benchmark.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.RawVsCompiled
        PRIVATE
        Menagerie::Common::Cuckoo
        Menagerie::Component::Savanna::Elephant
)
##############################################################################
