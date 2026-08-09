##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${Albatross}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)

target_include_directories(${Albatross}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Listeners.Tls
        PUBLIC
        ${Albatross}.Listeners.Base
        ${Albatross}.Listeners.ConnectionTracker
        ${Albatross}.Connection.Tls
        ${Albatross}.Types.Executor
        ${Albatross}.Config.TlsConfig
        ${Albatross}.Drivers.Concept
        ${Albatross}.Routing.Router
        Menagerie::Common::Cuckoo
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
