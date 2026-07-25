##############################################################################
# Http Types — static cold-path ResponseFactory
##############################################################################
add_library(${HTTP}.Types.ResponseFactory STATIC response_factory.cpp)

target_include_directories(${HTTP}.Types.ResponseFactory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.ResponseFactory PUBLIC
        ${HTTP}.Types.Response
        ${HTTP}.Types.Enums
)
##############################################################################
