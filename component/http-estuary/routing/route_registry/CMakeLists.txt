##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${Estuary}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${Estuary}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Routing.RouteRegistry
        PUBLIC
        ${Estuary}.Routing.Middleware
        ${Estuary}.Types.Enums
        ${Estuary}.Types.Errors
        Menagerie::Common::Beaver
        Boost::container
        Boost::unordered
        PRIVATE
        ${Estuary}.Types.UrlDecode
)
##############################################################################
