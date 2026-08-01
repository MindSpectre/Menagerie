##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${Http}.Routing.Controller STATIC controller.cpp)

target_include_directories(${Http}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Routing.Controller
        PUBLIC
        ${Http}.Routing.Middleware
        ${Http}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${Http}.Types.ErrorResponses
        Menagerie::Common::Beavers
        Menagerie::Common::Spider
        Menagerie::Common::Crow
        PRIVATE
        ${Http}.Routing.RouteRegistry
)
##############################################################################
