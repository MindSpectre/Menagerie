##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${Estuary}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${Estuary}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Routing.Middleware INTERFACE
        ${Estuary}.Types.AsyncOutcome
        ${Estuary}.Types.RequestContext
)
##############################################################################
