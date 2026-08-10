##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${Albatross}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${Albatross}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Routing.RouteRegistry
        PUBLIC
        ${Albatross}.Routing.Middleware
        ${Albatross}.Types.Enums
        ${Albatross}.Types.Errors
        Menagerie::Common::Beaver
        Boost::container
        Boost::unordered
        PRIVATE
        ${Albatross}.Types.UrlDecode
)
##############################################################################
