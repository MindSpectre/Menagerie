##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${Albatross}.Connection.Quic INTERFACE
        quic_connection.hpp
)

target_include_directories(${Albatross}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Connection.Quic INTERFACE
        ${Albatross}.Connection.RequestArena
        ${Albatross}.Connection.Concepts
        ${Albatross}.Types.Enums
        Boost::asio
)
##############################################################################
