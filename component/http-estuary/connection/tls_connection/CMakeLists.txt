##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${Estuary}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${Estuary}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Connection.Tls
        PUBLIC
        ${Estuary}.Connection.RequestArena
        ${Estuary}.Connection.Concepts
        ${Estuary}.Types.Enums
        ${Estuary}.Types.Executor
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
