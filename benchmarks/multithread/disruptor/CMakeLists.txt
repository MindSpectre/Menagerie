##############################################################################
# Benchmark Disruptor
##############################################################################
add_executable(${BENCHMARKS}.Multithread.Disruptor
        disruptor_speed_test.cpp
)
target_link_libraries(${BENCHMARKS}.Multithread.Disruptor
        PRIVATE
        Menagerie::Common::Starling
        Menagerie::Common::Chameleon
        Threads::Threads
)
target_compile_options(${BENCHMARKS}.Multithread.Disruptor PRIVATE -O3)
##############################################################################