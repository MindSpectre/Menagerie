add_library(${SAVANNA_ELEPHANT}.Capabilities.Executor.Shared INTERFACE
        shared/process_pgresult.hpp
        shared/executor_concept.hpp
)


target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Executor.Shared INTERFACE
        shared
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Executor.Shared INTERFACE
        ${SAVANNA}.Query.Compiler
        ${SAVANNA}.Base.Errors
        ${SAVANNA_ELEPHANT}.Errors
        ${SAVANNA_ELEPHANT}.Result
        ${SAVANNA_ELEPHANT}.Params
        Menagerie::Common::Beaver
)

##############################################################################
# Async Executor
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Capabilities.Executor.Async STATIC
        async_executor/postgres_async_executor.cpp
        async_executor/postgres_async_executor.hpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Executor.Async PUBLIC
        async_executor/
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Executor.Async PUBLIC
        ${SAVANNA_ELEPHANT}.Capabilities.Executor.Shared
        ${SAVANNA_ELEPHANT}.ConnectionHolder
        Boost::asio
)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Capabilities.Executor.Sync STATIC
        sync_executor/postgres_sync_executor.cpp
        sync_executor/postgres_sync_executor.hpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Executor.Sync PUBLIC
        sync_executor/
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Executor.Sync PUBLIC
        ${SAVANNA_ELEPHANT}.Capabilities.Executor.Shared
        ${SAVANNA_ELEPHANT}.ConnectionHolder
)
##############################################################################