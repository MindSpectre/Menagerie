##############################################################################
# Http Types — protocol-agnostic core types
#
# Each type is its own leaf target with its own include directory, so headers
# cross-reference each other as <body.hpp>/<headers.hpp>/... (never ../). The
# dotted ${Albatross}.Types target below is an INTERFACE aggregate that links
# every leaf — link it when you want the whole layer. There is intentionally
# no Menagerie::Component::Albatross::Types alias.
##############################################################################
add_subdirectory(enums)
add_subdirectory(executor)
add_subdirectory(async_outcome)
add_subdirectory(url_decode)
add_subdirectory(http_date)
add_subdirectory(headers)
add_subdirectory(errors)
add_subdirectory(body)
add_subdirectory(response)
add_subdirectory(request)
add_subdirectory(response_factory)
add_subdirectory(request_context)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${Albatross}.Types INTERFACE)

target_link_libraries(${Albatross}.Types INTERFACE
        ${Albatross}.Types.Enums
        ${Albatross}.Types.Executor
        ${Albatross}.Types.AsyncOutcome
        ${Albatross}.Types.UrlDecode
        ${Albatross}.Types.HttpDate
        ${Albatross}.Types.Headers
        ${Albatross}.Types.Errors
        ${Albatross}.Types.ErrorResponses
        ${Albatross}.Types.Body
        ${Albatross}.Types.Response
        ${Albatross}.Types.Request
        ${Albatross}.Types.ResponseFactory
        ${Albatross}.Types.RequestContext
)
##############################################################################
