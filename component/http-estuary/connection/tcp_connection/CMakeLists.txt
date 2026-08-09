##############################################################################
# Http Connection — TcpConnection (plain TCP over a strand-bound beast stream)
##############################################################################
add_library(${Estuary}.Connection.Tcp STATIC
        tcp_connection.cpp
)

target_include_directories(${Estuary}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Connection.Tcp
        PUBLIC
        ${Estuary}.Connection.RequestArena
        ${Estuary}.Connection.Concepts
        ${Estuary}.Types.Enums
        ${Estuary}.Types.Executor
        Boost::beast
        Boost::asio
)
##############################################################################
