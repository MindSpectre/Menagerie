##############################################################################
# Http Server — orchestration layer: the ServerObserver interface, the Server
# (executor-injected lifecycle orchestrator), and run_standalone convenience.
# Per-leaf convention; the dotted ${HTTP}.Server target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(server_observer)
add_subdirectory(server)
add_subdirectory(run_standalone)
add_subdirectory(attach_default_listeners)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${HTTP}.Server INTERFACE)

target_link_libraries(${HTTP}.Server INTERFACE
        ${HTTP}.Server.Observer
        ${HTTP}.Server.Core
        ${HTTP}.Server.RunStandalone
        ${HTTP}.Server.AttachDefaultListeners
)
##############################################################################
