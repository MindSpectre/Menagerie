##############################################################################
# Http Server — orchestration layer: the ServerObserver interface, the Server
# (executor-injected lifecycle orchestrator), and run_standalone convenience.
# Per-leaf convention; the dotted ${Estuary}.Server target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(server_observer)
add_subdirectory(server)
add_subdirectory(run_standalone)
add_subdirectory(attach_default_listeners)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Estuary}.Server INTERFACE)

target_link_libraries(${Estuary}.Server INTERFACE
        ${Estuary}.Server.Observer
        ${Estuary}.Server.Core
        ${Estuary}.Server.RunStandalone
        ${Estuary}.Server.AttachDefaultListeners
)
##############################################################################
