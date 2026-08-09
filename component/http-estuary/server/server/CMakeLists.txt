##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator)
##############################################################################
add_library(${Estuary}.Server.Core STATIC server.cpp)

target_include_directories(${Estuary}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Server.Core
        PUBLIC
        ${Estuary}.Server.Observer
        ${Estuary}.Types.Executor
        ${Estuary}.Routing
        ${Estuary}.Listeners
        ${Estuary}.Config
        Boost::asio
        Menagerie::Common::Cuckoo
        Menagerie::Common::Beaver
        Menagerie::Common::Spider
        Menagerie::Common::Crow
)
##############################################################################
