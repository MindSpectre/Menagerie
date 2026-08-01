##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${Http}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)

target_include_directories(${Http}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Listeners.Tls
        PUBLIC
        ${Http}.Listeners.Base
        ${Http}.Listeners.ConnectionTracker
        ${Http}.Connection.Tls
        ${Http}.Types.Executor
        ${Http}.Config.TlsConfig
        ${Http}.Drivers.Concept
        ${Http}.Routing.Router
        Menagerie::Common::Chrono
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
