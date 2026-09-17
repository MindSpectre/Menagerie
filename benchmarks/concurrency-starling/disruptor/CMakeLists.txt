##############################################################################
# Benchmark Disruptor
##############################################################################
add_executable(${BENCHMARKS}.Starling.Disruptor
        disruptor_speed_test.cpp
)
target_link_libraries(${BENCHMARKS}.Starling.Disruptor
        PRIVATE
        Menagerie::Common::Starling
        Menagerie::Common::Chameleon
        Threads::Threads
)
target_compile_options(${BENCHMARKS}.Starling.Disruptor PRIVATE -O3)
##############################################################################
# Opt-in: vcpkg supplies the SPSC queue dependency; the comparison target is
# still excluded unless explicitly requested.
option(MENAGERIE_BENCHMARK_RIGTORP_SPSC "Build the Disruptor/rigtorp SPSC queue comparison" OFF)
if (MENAGERIE_BENCHMARK_RIGTORP_SPSC)
    find_package(SPSCQueue CONFIG REQUIRED)

    add_executable(${BENCHMARKS}.Starling.SPSCQueue
            spsc_queue_benchmark.cpp
    )
    target_link_libraries(${BENCHMARKS}.Starling.SPSCQueue PRIVATE
            Menagerie::Common::Starling
            SPSCQueue::SPSCQueue
            Threads::Threads
    )
    target_compile_options(${BENCHMARKS}.Starling.SPSCQueue PRIVATE -O3)
    target_compile_definitions(${BENCHMARKS}.Starling.SPSCQueue PRIVATE
            MENAGERIE_RIGTORP_SPSC_PROVIDER="vcpkg:spscqueue"
    )
endif ()
