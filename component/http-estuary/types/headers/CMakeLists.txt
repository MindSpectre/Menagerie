##############################################################################
# Http Types — Headers (allocator-bound tagged union)
##############################################################################
add_library(${Estuary}.Types.Headers STATIC headers.cpp)

target_include_directories(${Estuary}.Types.Headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Headers PUBLIC
        Boost::beast
        Boost::container
        Menagerie::Common::Beaver
)
##############################################################################
