##############################################################################
# Concurrency Scenario Benchmarks
# One binary per subject (libpq, pqxx, SyncExecutor, LockFreeSession,
# BlockingSession). Each binary registers all 5 scenarios from
# bench_scenarios.hpp and sweeps WORKER_COUNTS.
##############################################################################

# Targets are added by later tasks in the benchmark refactor plan.

# --- LockFreeSession ---
add_executable(${BENCHMARKS_POSTGRESQL}.Scenarios.LockFreeSession
        benchmark_lock_free_session.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.Scenarios.LockFreeSession
        PRIVATE
        ${BENCHMARKS_POSTGRESQL}.Common
        Menagerie::Component::Database::PostgreSQL
)

# --- BlockingSession ---
add_executable(${BENCHMARKS_POSTGRESQL}.Scenarios.BlockingSession
        benchmark_blocking_session.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.Scenarios.BlockingSession
        PRIVATE
        ${BENCHMARKS_POSTGRESQL}.Common
        Menagerie::Component::Database::PostgreSQL
)

# --- libpq control ---
add_executable(${BENCHMARKS_POSTGRESQL}.Scenarios.LibPQ
        benchmark_libpq.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.Scenarios.LibPQ
        PRIVATE
        ${BENCHMARKS_POSTGRESQL}.Common
)

# --- pqxx control ---
add_executable(${BENCHMARKS_POSTGRESQL}.Scenarios.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.Scenarios.Pqxx
        PRIVATE
        ${BENCHMARKS_POSTGRESQL}.Common
        libpqxx::pqxx
)

# --- SyncExecutor control ---
add_executable(${BENCHMARKS_POSTGRESQL}.Scenarios.SyncExecutor
        benchmark_sync_executor.cpp
)
target_link_libraries(${BENCHMARKS_POSTGRESQL}.Scenarios.SyncExecutor
        PRIVATE
        ${BENCHMARKS_POSTGRESQL}.Common
        Menagerie::Component::Database::PostgreSQL
)
