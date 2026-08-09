##############################################################################
# PostgreSql Connection
##############################################################################
#add_library(${SAVANNA_ELEPHANT}.Connection STATIC
#
#)
#target_include_directories(${SAVANNA_ELEPHANT}.Connection PUBLIC
#        include
#)
#target_link_libraries(${SAVANNA_ELEPHANT}.Connection
#        PUBLIC
#        ${SAVANNA_ELEPHANT}.Config
#        ${SAVANNA_ELEPHANT}.Result
#        ${SAVANNA_ELEPHANT}.Params
#        ${SAVANNA_ELEPHANT}.Registries
#        ${SAVANNA}.Query.Compiler
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
add_library(${SAVANNA_ELEPHANT}.Capabilities INTERFACE
        provider/capability_provider.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Capabilities INTERFACE
        provider/
)
target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities INTERFACE
        ${SAVANNA_ELEPHANT}.Capabilities.Copier
        ${SAVANNA_ELEPHANT}.Capabilities.Notifier
        ${SAVANNA_ELEPHANT}.Capabilities.Executor.Async
        ${SAVANNA_ELEPHANT}.Capabilities.Executor.Sync
        ${SAVANNA_ELEPHANT}.Capabilities.Pipeline
)
##############################################################################