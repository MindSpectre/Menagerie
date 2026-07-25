##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${HTTP}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${HTTP}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Listeners.Tcp INTERFACE
        ${HTTP}.Listeners.Base
        ${HTTP}.Listeners.ConnectionTracker
        ${HTTP}.Connection.Tcp
        ${HTTP}.Types.Executor
        ${HTTP}.Drivers.Concept
        ${HTTP}.Routing.Router
        Menagerie::Common::Chrono
        Boost::beast
        Boost::asio
)
##############################################################################
