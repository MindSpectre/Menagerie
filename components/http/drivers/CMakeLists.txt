##############################################################################
# Http Drivers — the HttpDriver concept + protocol drivers (h1 real, h2/h3
# scaffolds). Per-leaf convention; the dotted ${HTTP}.Drivers target is an
# INTERFACE aggregate. Leaves are added by subsequent tasks.
##############################################################################
add_subdirectory(http_driver_concept)
add_subdirectory(http11)
add_subdirectory(http2)
add_subdirectory(http3)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${HTTP}.Drivers INTERFACE)

target_link_libraries(${HTTP}.Drivers INTERFACE
        ${HTTP}.Drivers.Concept
        ${HTTP}.Drivers.Http11
        ${HTTP}.Drivers.Http2
        ${HTTP}.Drivers.Http3
)
##############################################################################
