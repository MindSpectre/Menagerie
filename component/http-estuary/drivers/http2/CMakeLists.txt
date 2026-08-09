##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet)
##############################################################################
add_library(${Estuary}.Drivers.Http2 INTERFACE
        http2_driver.hpp
)

target_include_directories(${Estuary}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Drivers.Http2 INTERFACE
        ${Estuary}.Drivers.Concept
        ${Estuary}.Connection.Concepts
        ${Estuary}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
