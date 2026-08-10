##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${Albatross}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${Albatross}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Listeners.Tcp INTERFACE
        ${Albatross}.Listeners.Base
        ${Albatross}.Listeners.ConnectionTracker
        ${Albatross}.Connection.Tcp
        ${Albatross}.Types.Executor
        ${Albatross}.Drivers.Concept
        ${Albatross}.Routing.Router
        Menagerie::Common::Cuckoo
        Boost::beast
        Boost::asio
)
##############################################################################
