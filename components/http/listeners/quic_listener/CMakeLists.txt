##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${Http}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${Http}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Listeners.Quic INTERFACE
        ${Http}.Listeners.Base
        ${Http}.Types.Executor
        ${Http}.Drivers.Http3
        ${Http}.Config.TlsConfig
        ${Http}.Routing.Router
        Menagerie::Common::Crow
        Boost::asio
)
##############################################################################
