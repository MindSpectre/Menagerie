##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${Estuary}.Routing.Controller STATIC controller.cpp)

target_include_directories(${Estuary}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Routing.Controller
        PUBLIC
        ${Estuary}.Routing.Middleware
        ${Estuary}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${Estuary}.Types.ErrorResponses
        Menagerie::Common::Beaver
        Menagerie::Common::Spider
        Menagerie::Common::Crow
        PRIVATE
        ${Estuary}.Routing.RouteRegistry
)
##############################################################################
