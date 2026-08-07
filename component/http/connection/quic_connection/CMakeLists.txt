##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${Http}.Connection.Quic INTERFACE
        quic_connection.hpp
)

target_include_directories(${Http}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Connection.Quic INTERFACE
        ${Http}.Connection.RequestArena
        ${Http}.Connection.Concepts
        ${Http}.Types.Enums
        Boost::asio
)
##############################################################################
