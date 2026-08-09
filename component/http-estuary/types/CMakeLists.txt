##############################################################################
# Http Types — protocol-agnostic core types
#
# Each type is its own leaf target with its own include directory, so headers
# cross-reference each other as <body.hpp>/<headers.hpp>/... (never ../). The
# dotted ${Estuary}.Types target below is an INTERFACE aggregate that links
# every leaf — link it when you want the whole layer. There is intentionally
# no Menagerie::Component::Estuary::Types alias.
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
add_library(${Estuary}.Types INTERFACE)

target_link_libraries(${Estuary}.Types INTERFACE
        ${Estuary}.Types.Enums
        ${Estuary}.Types.Executor
        ${Estuary}.Types.AsyncOutcome
        ${Estuary}.Types.UrlDecode
        ${Estuary}.Types.HttpDate
        ${Estuary}.Types.Headers
        ${Estuary}.Types.Errors
        ${Estuary}.Types.ErrorResponses
        ${Estuary}.Types.Body
        ${Estuary}.Types.Response
        ${Estuary}.Types.Request
        ${Estuary}.Types.ResponseFactory
        ${Estuary}.Types.RequestContext
)
##############################################################################
