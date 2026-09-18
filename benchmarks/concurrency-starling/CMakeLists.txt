set(BENCHMARKS_STARLING ${BENCHMARKS}.Starling)
##############################################################################
# Multithread benchmarks
##############################################################################
if (BENCHMARK_RESOURCE_POOL)
    add_subdirectory(resource_pool)
endif ()

if (BENCHMARK_DISRUPTOR)
    add_subdirectory(disruptor)
endif ()
##############################################################################
