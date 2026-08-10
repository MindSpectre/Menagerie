##############################################################################
# Http Server — orchestration layer: the ServerObserver interface, the Server
# (executor-injected lifecycle orchestrator), and run_standalone convenience.
# Per-leaf convention; the dotted ${Albatross}.Server target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(server_observer)
add_subdirectory(server)
add_subdirectory(run_standalone)
add_subdirectory(attach_default_listeners)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Albatross}.Server INTERFACE)

target_link_libraries(${Albatross}.Server INTERFACE
        ${Albatross}.Server.Observer
        ${Albatross}.Server.Core
        ${Albatross}.Server.RunStandalone
        ${Albatross}.Server.AttachDefaultListeners
)
##############################################################################
