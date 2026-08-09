##############################################################################
# Http Drivers — the HttpDriver concept + protocol drivers (h1 real, h2/h3
# scaffolds). Per-leaf convention; the dotted ${Estuary}.Drivers target is an
# INTERFACE aggregate. Leaves are added by subsequent tasks.
##############################################################################
add_subdirectory(http_driver_concept)
add_subdirectory(http11)
add_subdirectory(http2)
add_subdirectory(http3)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Estuary}.Drivers INTERFACE)

target_link_libraries(${Estuary}.Drivers INTERFACE
        ${Estuary}.Drivers.Concept
        ${Estuary}.Drivers.Http11
        ${Estuary}.Drivers.Http2
        ${Estuary}.Drivers.Http3
)
##############################################################################
