##############################################################################
# Http Listeners — transport layer: the ListenerBase polymorphic seam, the
# connection tracker, and concrete listeners (TCP, TLS+ALPN, QUIC scaffold).
# Per-leaf convention; the dotted ${Estuary}.Listeners target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(listener_base)
add_subdirectory(connection_tracker)
add_subdirectory(tcp_listener)
add_subdirectory(tls_listener)
add_subdirectory(quic_listener)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Estuary}.Listeners INTERFACE)

target_link_libraries(${Estuary}.Listeners INTERFACE
        ${Estuary}.Listeners.Base
        ${Estuary}.Listeners.ConnectionTracker
        ${Estuary}.Listeners.Tcp
        ${Estuary}.Listeners.Tls
        ${Estuary}.Listeners.Quic
)
##############################################################################
