##############################################################################
# Concurrency Scenario Benchmarks
# One binary per subject (libpq, pqxx, SyncExecutor, Session). Each binary
# registers all 5 scenarios from bench_scenarios.hpp and sweeps WORKER_COUNTS;
# the Session binary registers each scenario twice, once per acquisition verb
# (try_with_async fail-fast vs co_await with_async FIFO parking).
##############################################################################

# --- Session (try + await acquisition verbs) ---
add_executable(${BENCHMARKS_ELEPHANT}.Scenarios.Session
        benchmark_session.cpp
)
target_link_libraries(${BENCHMARKS_ELEPHANT}.Scenarios.Session
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
