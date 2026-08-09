##############################################################################
# Http Types — static cold-path ResponseFactory
##############################################################################
add_library(${Albatross}.Types.ResponseFactory STATIC response_factory.cpp)

target_include_directories(${Albatross}.Types.ResponseFactory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.ResponseFactory PUBLIC
        ${Albatross}.Types.Response
        ${Albatross}.Types.Enums
)
##############################################################################
