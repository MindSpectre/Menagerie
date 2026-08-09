##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${Estuary}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)

target_include_directories(${Estuary}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Listeners.Tls
        PUBLIC
        ${Estuary}.Listeners.Base
        ${Estuary}.Listeners.ConnectionTracker
        ${Estuary}.Connection.Tls
        ${Estuary}.Types.Executor
        ${Estuary}.Config.TlsConfig
        ${Estuary}.Drivers.Concept
        ${Estuary}.Routing.Router
        Menagerie::Common::Cuckoo
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
