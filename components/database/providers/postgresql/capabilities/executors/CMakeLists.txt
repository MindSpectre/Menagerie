add_library(${DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared/process_pgresult.hpp
        shared/executor_concept.hpp
)


target_include_directories(${DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared
)

target_link_libraries(${DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        ${DATABASE}.Query.Compiler
        ${DATABASE}.Base.Errors
        ${DATABASE_POSTGRESQL}.Errors
        ${DATABASE_POSTGRESQL}.Result
        ${DATABASE_POSTGRESQL}.Params
        Menagerie::Common::Beavers
)

##############################################################################
# Async Executor
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Capabilities.Executor.Async STATIC
        async_executor/postgres_async_executor.cpp
        async_executor/postgres_async_executor.hpp
)

target_include_directories(${DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        async_executor/
)

target_link_libraries(${DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        ${DATABASE_POSTGRESQL}.Capabilities.Executor.Shared
        ${DATABASE_POSTGRESQL}.ConnectionHolder
        Boost::asio
)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Capabilities.Executor.Sync STATIC
        sync_executor/postgres_sync_executor.cpp
        sync_executor/postgres_sync_executor.hpp
)

target_include_directories(${DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        sync_executor/
)

target_link_libraries(${DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        ${DATABASE_POSTGRESQL}.Capabilities.Executor.Shared
        ${DATABASE_POSTGRESQL}.ConnectionHolder
)
##############################################################################