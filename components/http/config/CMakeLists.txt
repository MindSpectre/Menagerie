##############################################################################
# Http Config — configuration value types + JSON loading.
# Per-leaf convention; the dotted ${HTTP}.Config target is an INTERFACE
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
add_library(${HTTP}.Config INTERFACE)

target_link_libraries(${HTTP}.Config INTERFACE
        ${HTTP}.Config.TlsConfig
        ${HTTP}.Config.ServerConfig
        ${HTTP}.Config.Timeouts
        ${HTTP}.Config.ListenerConfig
        ${HTTP}.Config.LoadServerConfig
)
##############################################################################
