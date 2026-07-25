##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet)
##############################################################################
add_library(${HTTP}.Drivers.Http2 INTERFACE
        http2_driver.hpp
)

target_include_directories(${HTTP}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Drivers.Http2 INTERFACE
        ${HTTP}.Drivers.Concept
        ${HTTP}.Connection.Concepts
        ${HTTP}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
