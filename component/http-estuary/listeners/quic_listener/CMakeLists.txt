##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${Estuary}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${Estuary}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Listeners.Quic INTERFACE
        ${Estuary}.Listeners.Base
        ${Estuary}.Types.Executor
        ${Estuary}.Drivers.Http3
        ${Estuary}.Config.TlsConfig
        ${Estuary}.Routing.Router
        Menagerie::Common::Crow
        Boost::asio
)
##############################################################################
