##############################################################################
# Http Types — static cold-path ResponseFactory
##############################################################################
add_library(${Http}.Types.ResponseFactory STATIC response_factory.cpp)

target_include_directories(${Http}.Types.ResponseFactory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.ResponseFactory PUBLIC
        ${Http}.Types.Response
        ${Http}.Types.Enums
)
##############################################################################
