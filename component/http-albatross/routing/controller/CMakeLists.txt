##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${Albatross}.Routing.Controller STATIC controller.cpp)

target_include_directories(${Albatross}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Routing.Controller
        PUBLIC
        ${Albatross}.Routing.Middleware
        ${Albatross}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${Albatross}.Types.ErrorResponses
        Menagerie::Common::Beaver
        Menagerie::Common::Spider
        Menagerie::Common::Crow
        PRIVATE
        ${Albatross}.Routing.RouteRegistry
)
##############################################################################
