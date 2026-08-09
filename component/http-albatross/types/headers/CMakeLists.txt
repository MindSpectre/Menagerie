##############################################################################
# Http Types — Headers (allocator-bound tagged union)
##############################################################################
add_library(${Albatross}.Types.Headers STATIC headers.cpp)

target_include_directories(${Albatross}.Types.Headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.Headers PUBLIC
        Boost::beast
        Boost::container
        Menagerie::Common::Beaver
)
##############################################################################
