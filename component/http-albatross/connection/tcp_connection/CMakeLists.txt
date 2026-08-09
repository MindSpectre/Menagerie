##############################################################################
# Http Connection — TcpConnection (plain TCP over a strand-bound beast stream)
##############################################################################
add_library(${Albatross}.Connection.Tcp STATIC
        tcp_connection.cpp
)

target_include_directories(${Albatross}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Connection.Tcp
        PUBLIC
        ${Albatross}.Connection.RequestArena
        ${Albatross}.Connection.Concepts
        ${Albatross}.Types.Enums
        ${Albatross}.Types.Executor
        Boost::beast
        Boost::asio
)
##############################################################################
