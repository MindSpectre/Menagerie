##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${Estuary}.Connection target is an INTERFACE aggregate. Leaves are
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
add_library(${Estuary}.Connection INTERFACE)

target_link_libraries(${Estuary}.Connection INTERFACE
        ${Estuary}.Connection.RequestArena
        ${Estuary}.Connection.Concepts
        ${Estuary}.Connection.Tcp
        ${Estuary}.Connection.Quic
        ${Estuary}.Connection.Tls
)
##############################################################################
