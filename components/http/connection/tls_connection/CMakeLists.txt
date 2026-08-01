##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${Http}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${Http}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Connection.Tls
        PUBLIC
        ${Http}.Connection.RequestArena
        ${Http}.Connection.Concepts
        ${Http}.Types.Enums
        ${Http}.Types.Executor
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
