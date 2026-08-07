##############################################################################
# Http Connection — TcpConnection (plain TCP over a strand-bound beast stream)
##############################################################################
add_library(${Http}.Connection.Tcp STATIC
        tcp_connection.cpp
)

target_include_directories(${Http}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Connection.Tcp
        PUBLIC
        ${Http}.Connection.RequestArena
        ${Http}.Connection.Concepts
        ${Http}.Types.Enums
        ${Http}.Types.Executor
        Boost::beast
        Boost::asio
)
##############################################################################
