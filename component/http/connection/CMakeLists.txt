##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${Http}.Connection target is an INTERFACE aggregate. Leaves are
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
add_library(${Http}.Connection INTERFACE)

target_link_libraries(${Http}.Connection INTERFACE
        ${Http}.Connection.RequestArena
        ${Http}.Connection.Concepts
        ${Http}.Connection.Tcp
        ${Http}.Connection.Quic
        ${Http}.Connection.Tls
)
##############################################################################
