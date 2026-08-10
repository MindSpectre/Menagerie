##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${Albatross}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${Albatross}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Listeners.Quic INTERFACE
        ${Albatross}.Listeners.Base
        ${Albatross}.Types.Executor
        ${Albatross}.Drivers.Http3
        ${Albatross}.Config.TlsConfig
        ${Albatross}.Routing.Router
        Menagerie::Common::Crow
        Boost::asio
)
##############################################################################
