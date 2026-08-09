##############################################################################
# Http Types — static cold-path ResponseFactory
##############################################################################
add_library(${Estuary}.Types.ResponseFactory STATIC response_factory.cpp)

target_include_directories(${Estuary}.Types.ResponseFactory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.ResponseFactory PUBLIC
        ${Estuary}.Types.Response
        ${Estuary}.Types.Enums
)
##############################################################################
