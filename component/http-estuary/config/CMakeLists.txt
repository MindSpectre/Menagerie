##############################################################################
# Http Config — configuration value types + JSON loading.
# Per-leaf convention; the dotted ${Estuary}.Config target is an INTERFACE
# aggregate.
##############################################################################
add_subdirectory(tls_config)
add_subdirectory(server_config)
add_subdirectory(timeouts)
add_subdirectory(listener_config)
add_subdirectory(load_server_config)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Estuary}.Config INTERFACE)

target_link_libraries(${Estuary}.Config INTERFACE
        ${Estuary}.Config.TlsConfig
        ${Estuary}.Config.ServerConfig
        ${Estuary}.Config.Timeouts
        ${Estuary}.Config.ListenerConfig
        ${Estuary}.Config.LoadServerConfig
)
##############################################################################
