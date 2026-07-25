##############################################################################
# Http Connection — TcpConnection (plain TCP over a strand-bound beast stream)
##############################################################################
add_library(${HTTP}.Connection.Tcp STATIC
        tcp_connection.cpp
)

target_include_directories(${HTTP}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Connection.Tcp
        PUBLIC
        ${HTTP}.Connection.RequestArena
        ${HTTP}.Connection.Concepts
        ${HTTP}.Types.Enums
        ${HTTP}.Types.Executor
        Boost::beast
        Boost::asio
)
##############################################################################
