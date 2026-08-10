##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator)
##############################################################################
add_library(${Albatross}.Server.Core STATIC server.cpp)

target_include_directories(${Albatross}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Server.Core
        PUBLIC
        ${Albatross}.Server.Observer
        ${Albatross}.Types.Executor
        ${Albatross}.Routing
        ${Albatross}.Listeners
        ${Albatross}.Config
        Boost::asio
        Menagerie::Common::Cuckoo
        Menagerie::Common::Beaver
        Menagerie::Common::Spider
        Menagerie::Common::Crow
)
##############################################################################
