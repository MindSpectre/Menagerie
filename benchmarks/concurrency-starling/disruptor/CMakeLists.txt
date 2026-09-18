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
# vcpkg's starling-benchmarks feature supplies the reference queue.
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
