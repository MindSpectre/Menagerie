##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator)
##############################################################################
add_library(${HTTP}.Server.Core STATIC server.cpp)

target_include_directories(${HTTP}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Server.Core
        PUBLIC
        ${HTTP}.Server.Observer
        ${HTTP}.Types.Executor
        ${HTTP}.Routing
        ${HTTP}.Listeners
        ${HTTP}.Config
        Boost::asio
        Menagerie::Common::Chrono
        Menagerie::Common::Beavers
        Menagerie::Common::Spider
        Menagerie::Common::Crow
)
##############################################################################
