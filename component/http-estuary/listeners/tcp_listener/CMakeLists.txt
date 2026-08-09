##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${Estuary}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${Estuary}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Listeners.Tcp INTERFACE
        ${Estuary}.Listeners.Base
        ${Estuary}.Listeners.ConnectionTracker
        ${Estuary}.Connection.Tcp
        ${Estuary}.Types.Executor
        ${Estuary}.Drivers.Concept
        ${Estuary}.Routing.Router
        Menagerie::Common::Cuckoo
        Boost::beast
        Boost::asio
)
##############################################################################
