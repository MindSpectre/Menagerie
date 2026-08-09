##############################################################################
# Concurrency Scenario Benchmarks
# One binary per subject (libpq, pqxx, SyncExecutor, LockFreeSession,
# BlockingSession). Each binary registers all 5 scenarios from
# bench_scenarios.hpp and sweeps WORKER_COUNTS.
##############################################################################

# Targets are added by later tasks in the benchmark refactor plan.

# --- LockFreeSession ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.LockFreeSession
        benchmark_lock_free_session.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.LockFreeSession
        PRIVATE
        ${BENCHMARKS_ELEPHANT}.Common
        Menagerie::Component::Savanna::Elephant
)

# --- BlockingSession ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.BlockingSession
        benchmark_blocking_session.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.BlockingSession
        PRIVATE
        ${BENCHMARKS_ELEPHANT}.Common
        Menagerie::Component::Savanna::Elephant
)

# --- libpq control ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.LibPQ
        benchmark_libpq.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.LibPQ
        PRIVATE
        ${BENCHMARKS_ELEPHANT}.Common
)

# --- pqxx control ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.Pqxx
        PRIVATE
        ${BENCHMARKS_ELEPHANT}.Common
        libpqxx::pqxx
)

# --- SyncExecutor control ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.SyncExecutor
        benchmark_sync_executor.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.SyncExecutor
        PRIVATE
        ${BENCHMARKS_ELEPHANT}.Common
        Menagerie::Component::Savanna::Elephant
)
