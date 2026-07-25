##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${HTTP}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${HTTP}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Listeners.Quic INTERFACE
        ${HTTP}.Listeners.Base
        ${HTTP}.Types.Executor
        ${HTTP}.Drivers.Http3
        ${HTTP}.Config.TlsConfig
        ${HTTP}.Routing.Router
        Menagerie::Common::Crow
        Boost::asio
)
##############################################################################
