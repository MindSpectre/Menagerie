##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${HTTP}.Connection target is an INTERFACE aggregate. Leaves are
# added by subsequent tasks.
##############################################################################
add_subdirectory(request_arena)
add_subdirectory(concepts)
add_subdirectory(tcp_connection)
add_subdirectory(quic_connection)
add_subdirectory(tls_connection)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${HTTP}.Connection INTERFACE)

target_link_libraries(${HTTP}.Connection INTERFACE
        ${HTTP}.Connection.RequestArena
        ${HTTP}.Connection.Concepts
        ${HTTP}.Connection.Tcp
        ${HTTP}.Connection.Quic
        ${HTTP}.Connection.Tls
)
##############################################################################
