##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${Http}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${Http}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Listeners.Tcp INTERFACE
        ${Http}.Listeners.Base
        ${Http}.Listeners.ConnectionTracker
        ${Http}.Connection.Tcp
        ${Http}.Types.Executor
        ${Http}.Drivers.Concept
        ${Http}.Routing.Router
        Menagerie::Common::Chrono
        Boost::beast
        Boost::asio
)
##############################################################################
