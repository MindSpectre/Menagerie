##############################################################################
# PostgreSql Connection
##############################################################################
#add_library(${REEF_POSTGRESQL}.Connection STATIC
#
#)
#target_include_directories(${REEF_POSTGRESQL}.Connection PUBLIC
#        include
#)
#target_link_libraries(${REEF_POSTGRESQL}.Connection
#        PUBLIC
#        ${REEF_POSTGRESQL}.Config
#        ${REEF_POSTGRESQL}.Result
#        ${REEF_POSTGRESQL}.Params
#        ${REEF_POSTGRESQL}.Registries
#        ${REEF}.Query.Compiler
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
add_library(${REEF_POSTGRESQL}.Capabilities INTERFACE
        provider/capability_provider.hpp
)
target_include_directories(${REEF_POSTGRESQL}.Capabilities INTERFACE
        provider/
)
target_link_libraries(${REEF_POSTGRESQL}.Capabilities INTERFACE
        ${REEF_POSTGRESQL}.Capabilities.Copier
        ${REEF_POSTGRESQL}.Capabilities.Notifier
        ${REEF_POSTGRESQL}.Capabilities.Executor.Async
        ${REEF_POSTGRESQL}.Capabilities.Executor.Sync
        ${REEF_POSTGRESQL}.Capabilities.Pipeline
)
##############################################################################