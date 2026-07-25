##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${HTTP}.Connection.Quic INTERFACE
        quic_connection.hpp
)

target_include_directories(${HTTP}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Connection.Quic INTERFACE
        ${HTTP}.Connection.RequestArena
        ${HTTP}.Connection.Concepts
        ${HTTP}.Types.Enums
        Boost::asio
)
##############################################################################
