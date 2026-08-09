##############################################################################
# Http Config — configuration value types + JSON loading.
# Per-leaf convention; the dotted ${Albatross}.Config target is an INTERFACE
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
add_library(${Albatross}.Config INTERFACE)

target_link_libraries(${Albatross}.Config INTERFACE
        ${Albatross}.Config.TlsConfig
        ${Albatross}.Config.ServerConfig
        ${Albatross}.Config.Timeouts
        ${Albatross}.Config.ListenerConfig
        ${Albatross}.Config.LoadServerConfig
)
##############################################################################
