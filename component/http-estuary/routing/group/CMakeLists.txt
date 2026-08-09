##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${Estuary}.Routing.Group INTERFACE group.hpp)

target_include_directories(${Estuary}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Routing.Group INTERFACE
        ${Estuary}.Routing.Controller
        ${Estuary}.Routing.RouteRegistry
)
##############################################################################
