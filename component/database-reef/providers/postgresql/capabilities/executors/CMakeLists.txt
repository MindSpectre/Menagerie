add_library(${REEF_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared/process_pgresult.hpp
        shared/executor_concept.hpp
)


target_include_directories(${REEF_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared
)

target_link_libraries(${REEF_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        ${REEF}.Query.Compiler
        ${REEF}.Base.Errors
        ${REEF_POSTGRESQL}.Errors
        ${REEF_POSTGRESQL}.Result
        ${REEF_POSTGRESQL}.Params
        Menagerie::Common::Beaver
)

##############################################################################
# Async Executor
##############################################################################
add_library(${REEF_POSTGRESQL}.Capabilities.Executor.Async STATIC
        async_executor/postgres_async_executor.cpp
        async_executor/postgres_async_executor.hpp
)

target_include_directories(${REEF_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        async_executor/
)

target_link_libraries(${REEF_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        ${REEF_POSTGRESQL}.Capabilities.Executor.Shared
        ${REEF_POSTGRESQL}.ConnectionHolder
        Boost::asio
)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${REEF_POSTGRESQL}.Capabilities.Executor.Sync STATIC
        sync_executor/postgres_sync_executor.cpp
        sync_executor/postgres_sync_executor.hpp
)

target_include_directories(${REEF_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        sync_executor/
)

target_link_libraries(${REEF_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        ${REEF_POSTGRESQL}.Capabilities.Executor.Shared
        ${REEF_POSTGRESQL}.ConnectionHolder
)
##############################################################################