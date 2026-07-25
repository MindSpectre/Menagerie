##############################################################################
# Http Drivers — Http11Driver (real HTTP/1.1 over Boost.Beast)
##############################################################################
add_library(${HTTP}.Drivers.Http11 STATIC
        http11_driver.cpp
        http11_driver.hpp
        http11_config.hpp
)

target_include_directories(${HTTP}.Drivers.Http11 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Drivers.Http11
        PUBLIC
        ${HTTP}.Types
        ${HTTP}.Routing.Router
        ${HTTP}.Connection.Concepts
        Boost::beast
        Boost::asio
)
##############################################################################
