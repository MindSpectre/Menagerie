##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet)
##############################################################################
add_library(${Albatross}.Drivers.Http2 INTERFACE
        http2_driver.hpp
)

target_include_directories(${Albatross}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Drivers.Http2 INTERFACE
        ${Albatross}.Drivers.Concept
        ${Albatross}.Connection.Concepts
        ${Albatross}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
