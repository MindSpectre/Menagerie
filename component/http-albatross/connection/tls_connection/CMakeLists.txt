##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${Albatross}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${Albatross}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Connection.Tls
        PUBLIC
        ${Albatross}.Connection.RequestArena
        ${Albatross}.Connection.Concepts
        ${Albatross}.Types.Enums
        ${Albatross}.Types.Executor
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
