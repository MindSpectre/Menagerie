set(BENCHMARKS_POSTGRESQL ${BENCHMARKS}.Database.PostgreSQL)
##############################################################################
# PostgreSQL Client Overhead Benchmarks
# Requires vcpkg feature 'pg-benchmarks' (Google Benchmark + libpqxx)
##############################################################################
find_package(benchmark CONFIG QUIET)
find_package(libpqxx CONFIG QUIET)

if (benchmark_FOUND AND libpqxx_FOUND)

    # ─── Shared fixture library ──────────────────────────────────────────
    add_library(${BENCHMARKS_POSTGRESQL}.Common STATIC
            common/bench_fixture.cpp
    )
    target_include_directories(${BENCHMARKS_POSTGRESQL}.Common PUBLIC common)
    target_link_libraries(${BENCHMARKS_POSTGRESQL}.Common
            PUBLIC
            benchmark::benchmark
            benchmark::benchmark_main
            PostgreSQL::PostgreSQL
            Boost::headers
            Menagerie::Common::Beavers
    )

    add_subdirectory(concurrency_scenarios)

else ()
    if (NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found — skipping PG overhead benchmarks (enable vcpkg feature 'pg-benchmarks')")
    endif ()
    if (NOT libpqxx_FOUND)
        message(STATUS "libpqxx not found — skipping PG overhead benchmarks (enable vcpkg feature 'pg-benchmarks')")
    endif ()
endif ()

##############################################################################
# Benchmark Raw String vs Compiled Query
##############################################################################
add_executable(${BENCHMARKS_POSTGRESQL}.RawVsCompiled
        raw_vs_compiled_benchmark.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.RawVsCompiled
        PRIVATE
        Menagerie::Common::Chrono
        Menagerie::Component::Database::PostgreSQL
)
##############################################################################
