##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${Albatross}.Connection target is an INTERFACE aggregate. Leaves are
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
add_library(${Albatross}.Connection INTERFACE)

target_link_libraries(${Albatross}.Connection INTERFACE
        ${Albatross}.Connection.RequestArena
        ${Albatross}.Connection.Concepts
        ${Albatross}.Connection.Tcp
        ${Albatross}.Connection.Quic
        ${Albatross}.Connection.Tls
)
##############################################################################
