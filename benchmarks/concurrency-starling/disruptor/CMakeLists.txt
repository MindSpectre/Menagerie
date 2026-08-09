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