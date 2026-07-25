##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${HTTP}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${HTTP}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Connection.Tls
        PUBLIC
        ${HTTP}.Connection.RequestArena
        ${HTTP}.Connection.Concepts
        ${HTTP}.Types.Enums
        ${HTTP}.Types.Executor
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
