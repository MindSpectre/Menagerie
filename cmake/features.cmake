option(USE_BOOST "Enable Boost library" ON)
option(BUILD_COMPONENTS "Build components" ON)
option(USE_TESTS "Tests activated" ON)
option(DO_BENCHMARKS "Benchmarks activated" ON)
option(BUILD_EXAMPLES "Build example apps" ON)
option(ENABLE_LOGGING "Enable end user logging" ON)
option(KEEP_FRAME_POINTERS "Keep frame pointers in Release (perf call graphs)" OFF)
option(BUILD_DOCS "Generate API documentation with Doxygen (target: docs)" OFF)
option(UNRECOVERABLE_EXCEPTIONS_TERMINATE
        "UNRECOVERABLE_NOEXCEPT expands to noexcept: unrecoverable exceptions (bad_alloc) terminate instead of unwinding" ON)

if (ENABLE_LOGGING)
    option(COMPONENT_LOGGING "Enable logging in components" ON)
endif ()

#Component option
if (BUILD_COMPONENTS)
    option(BUILD_HTTP "Build http" ON)
    option(BUILD_DATABASE "Build database" ON)
endif ()

if (BUILD_DATABASE)
    option(BUILD_POSTGRESQL "Build PostgreSQL" ON)
endif ()
