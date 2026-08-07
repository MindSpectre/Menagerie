##############################################################################
# Http Drivers — the HttpDriver concept + protocol drivers (h1 real, h2/h3
# scaffolds). Per-leaf convention; the dotted ${Http}.Drivers target is an
# INTERFACE aggregate. Leaves are added by subsequent tasks.
##############################################################################
add_subdirectory(http_driver_concept)
add_subdirectory(http11)
add_subdirectory(http2)
add_subdirectory(http3)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Http}.Drivers INTERFACE)

target_link_libraries(${Http}.Drivers INTERFACE
        ${Http}.Drivers.Concept
        ${Http}.Drivers.Http11
        ${Http}.Drivers.Http2
        ${Http}.Drivers.Http3
)
##############################################################################
