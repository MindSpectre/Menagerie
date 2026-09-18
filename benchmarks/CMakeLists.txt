set(BENCHMARKS ${PROJECT_NAME}.Benchmarks)

if (BENCHMARK_CROW_LOGGER)
    add_subdirectory(logger-crow)
endif ()

if (BENCHMARK_RESOURCE_POOL OR BENCHMARK_DISRUPTOR)
    add_subdirectory(concurrency-starling)
endif ()

if (BUILD_HTTP AND BENCHMARK_HTTP)
    message("Benchmarks for Http will be built")
    add_subdirectory(http-albatross)
endif ()

if (BUILD_DATABASE AND BENCHMARK_POSTGRESQL)
    message("Benchmarks for Database will be built")
    add_subdirectory(database-savanna)
endif ()
