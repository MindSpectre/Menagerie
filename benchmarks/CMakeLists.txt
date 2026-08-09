set(BENCHMARKS ${PROJECT_NAME}.Benchmarks)

add_subdirectory(logger-crow)

add_subdirectory(concurrency-starling)

if (BUILD_HTTP)
    message("Benchmarks for Http will be built")
    add_subdirectory(http-estuary)
endif ()

if (BUILD_DATABASE)
    message("Benchmarks for Database will be built")
    add_subdirectory(database-reef)
endif ()