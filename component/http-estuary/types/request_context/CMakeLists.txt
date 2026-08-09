##############################################################################
# Http Types — RequestContext (accessors, params, bag, ctx factories)
##############################################################################
add_library(${Estuary}.Types.RequestContext STATIC request_context.cpp)

target_include_directories(${Estuary}.Types.RequestContext PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.RequestContext
        PUBLIC
        ${Estuary}.Types.Request
        ${Estuary}.Types.Response
        ${Estuary}.Types.Body
        ${Estuary}.Types.Headers
        ${Estuary}.Types.Enums
        Boost::container
        PRIVATE
        ${Estuary}.Types.UrlDecode
)
##############################################################################
