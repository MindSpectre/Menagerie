##############################################################################
# PostgreSql Connection
##############################################################################
#add_library(${DATABASE_POSTGRESQL}.Connection STATIC
#
#)
#target_include_directories(${DATABASE_POSTGRESQL}.Connection PUBLIC
#        include
#)
#target_link_libraries(${DATABASE_POSTGRESQL}.Connection
#        PUBLIC
#        ${DATABASE_POSTGRESQL}.Config
#        ${DATABASE_POSTGRESQL}.Result
#        ${DATABASE_POSTGRESQL}.Params
#        ${DATABASE_POSTGRESQL}.Registries
#        ${DATABASE}.Query.Compiler
#        Boost::asio
#)
##############################################################################

add_subdirectory(copier)

add_subdirectory(executors)

add_subdirectory(notifier)

add_subdirectory(pipeline)

##############################################################################
# PostgreSql Capabilities + Provider Concept
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Capabilities INTERFACE
        provider/capability_provider.hpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Capabilities INTERFACE
        provider/
)
target_link_libraries(${DATABASE_POSTGRESQL}.Capabilities INTERFACE
        ${DATABASE_POSTGRESQL}.Capabilities.Copier
        ${DATABASE_POSTGRESQL}.Capabilities.Notifier
        ${DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DATABASE_POSTGRESQL}.Capabilities.Pipeline
)
##############################################################################