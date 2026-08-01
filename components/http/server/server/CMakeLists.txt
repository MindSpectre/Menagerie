##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator)
##############################################################################
add_library(${Http}.Server.Core STATIC server.cpp)

target_include_directories(${Http}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Server.Core
        PUBLIC
        ${Http}.Server.Observer
        ${Http}.Types.Executor
        ${Http}.Routing
        ${Http}.Listeners
        ${Http}.Config
        Boost::asio
        Menagerie::Common::Chrono
        Menagerie::Common::Beavers
        Menagerie::Common::Spider
        Menagerie::Common::Crow
)
##############################################################################
