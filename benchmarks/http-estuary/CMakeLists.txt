##############################################################################
# Http/1.1 saturating load generator. Pure Asio client — no menagerie types.
# Links the umbrella for Boost propagation and so a non-test consumer exercises
# Menagerie::Component::Estuary.
##############################################################################
add_executable(${BENCHMARKS}.Estuary.Bomber
        http_bomber.cpp
)
target_link_libraries(${BENCHMARKS}.Estuary.Bomber
        PRIVATE
        Menagerie::Component::Estuary
        Threads::Threads
)
##############################################################################

##############################################################################
# The server under load — new-API counterpart the bomber points at.
##############################################################################
add_executable(${BENCHMARKS}.Estuary.BenchServer
        bench_server.cpp
)
target_link_libraries(${BENCHMARKS}.Estuary.BenchServer
        PRIVATE
        Menagerie::Component::Estuary
)
##############################################################################

##############################################################################
# Drogon reference server — the yardstick bench_server is measured against.
# Requires vcpkg feature 'drogon-benchmarks'.
##############################################################################
find_package(Drogon CONFIG QUIET)

if (Drogon_FOUND)
    message(STATUS "Drogon found — building Http reference server")
    add_executable(${BENCHMARKS}.Estuary.DrogonBenchServer
            drogon_bench_server.cpp
    )
    target_link_libraries(${BENCHMARKS}.Estuary.DrogonBenchServer
            PRIVATE
            Drogon::Drogon
    )
else ()
    message(STATUS "Drogon not found — skipping Http reference server (enable vcpkg feature 'drogon-benchmarks')")
endif ()
##############################################################################
