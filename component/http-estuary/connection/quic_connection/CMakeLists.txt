##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${Estuary}.Connection.Quic INTERFACE
        quic_connection.hpp
)

target_include_directories(${Estuary}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Connection.Quic INTERFACE
        ${Estuary}.Connection.RequestArena
        ${Estuary}.Connection.Concepts
        ${Estuary}.Types.Enums
        Boost::asio
)
##############################################################################
