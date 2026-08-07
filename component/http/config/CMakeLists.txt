##############################################################################
# Http Config — configuration value types + JSON loading.
# Per-leaf convention; the dotted ${Http}.Config target is an INTERFACE
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
add_library(${Http}.Config INTERFACE)

target_link_libraries(${Http}.Config INTERFACE
        ${Http}.Config.TlsConfig
        ${Http}.Config.ServerConfig
        ${Http}.Config.Timeouts
        ${Http}.Config.ListenerConfig
        ${Http}.Config.LoadServerConfig
)
##############################################################################
