##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${HTTP}.Routing.Controller STATIC controller.cpp)

target_include_directories(${HTTP}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Routing.Controller
        PUBLIC
        ${HTTP}.Routing.Middleware
        ${HTTP}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${HTTP}.Types.ErrorResponses
        Menagerie::Common::Beavers
        Menagerie::Common::Spider
        Menagerie::Common::Crow
        PRIVATE
        ${HTTP}.Routing.RouteRegistry
)
##############################################################################
