##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${HTTP}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)

target_include_directories(${HTTP}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Listeners.Tls
        PUBLIC
        ${HTTP}.Listeners.Base
        ${HTTP}.Listeners.ConnectionTracker
        ${HTTP}.Connection.Tls
        ${HTTP}.Types.Executor
        ${HTTP}.Config.TlsConfig
        ${HTTP}.Drivers.Concept
        ${HTTP}.Routing.Router
        Menagerie::Common::Chrono
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
