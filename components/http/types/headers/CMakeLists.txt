##############################################################################
# Http Types — Headers (allocator-bound tagged union)
##############################################################################
add_library(${HTTP}.Types.Headers STATIC headers.cpp)

target_include_directories(${HTTP}.Types.Headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Headers PUBLIC
        Boost::beast
        Boost::container
        Menagerie::Common::Beavers
)
##############################################################################
