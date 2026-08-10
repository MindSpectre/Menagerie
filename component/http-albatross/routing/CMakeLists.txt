##############################################################################
# Http Routing — registry, controllers, bake step, groups, router
#
# Same convention as types/: each thing is its own leaf target owning its
# include dir (headers cross-reference as <route_registry.hpp>, never ../).
# The dotted ${Albatross}.Routing target below is an INTERFACE aggregate.
##############################################################################
add_subdirectory(middleware)
add_subdirectory(route_registry)
add_subdirectory(controller)
add_subdirectory(group)
add_subdirectory(router)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Albatross}.Routing INTERFACE)

target_link_libraries(${Albatross}.Routing INTERFACE
        ${Albatross}.Routing.Middleware
        ${Albatross}.Routing.RouteRegistry
        ${Albatross}.Routing.Controller
        ${Albatross}.Routing.Group
        ${Albatross}.Routing.Router
)
##############################################################################
