##############################################################################
# Http Types — RequestContext (accessors, params, bag, ctx factories)
##############################################################################
add_library(${HTTP}.Types.RequestContext STATIC request_context.cpp)

target_include_directories(${HTTP}.Types.RequestContext PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.RequestContext
        PUBLIC
        ${HTTP}.Types.Request
        ${HTTP}.Types.Response
        ${HTTP}.Types.Body
        ${HTTP}.Types.Headers
        ${HTTP}.Types.Enums
        Boost::container
        PRIVATE
        ${HTTP}.Types.UrlDecode
)
##############################################################################
