##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${Http}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${Http}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Routing.Middleware INTERFACE
        ${Http}.Types.AsyncOutcome
        ${Http}.Types.RequestContext
)
##############################################################################
