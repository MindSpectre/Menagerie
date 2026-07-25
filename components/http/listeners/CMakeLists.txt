##############################################################################
# Http Listeners — transport layer: the ListenerBase polymorphic seam, the
# connection tracker, and concrete listeners (TCP, TLS+ALPN, QUIC scaffold).
# Per-leaf convention; the dotted ${HTTP}.Listeners target is an INTERFACE
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
add_library(${HTTP}.Listeners INTERFACE)

target_link_libraries(${HTTP}.Listeners INTERFACE
        ${HTTP}.Listeners.Base
        ${HTTP}.Listeners.ConnectionTracker
        ${HTTP}.Listeners.Tcp
        ${HTTP}.Listeners.Tls
        ${HTTP}.Listeners.Quic
)
##############################################################################
