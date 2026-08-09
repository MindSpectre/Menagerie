##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet)
##############################################################################
add_library(${Albatross}.Drivers.Http3 INTERFACE
        http3_driver.hpp
)

target_include_directories(${Albatross}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Drivers.Http3 INTERFACE
        ${Albatross}.Drivers.Concept
        ${Albatross}.Connection.Concepts
        ${Albatross}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
