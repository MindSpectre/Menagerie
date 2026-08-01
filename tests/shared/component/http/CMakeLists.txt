add_library(${SHARED_TESTING_TARGET}.Http INTERFACE
        http_test_fixture.hpp
        server_test_fixture.hpp
        test_tls_cert.hpp
        tls_client.hpp
)

target_include_directories(${SHARED_TESTING_TARGET}.Http INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SHARED_TESTING_TARGET}.Http INTERFACE
        Menagerie.Component.HTTP.Listeners
        Menagerie.Component.HTTP.Drivers
        Menagerie.Component.HTTP.Routing
        Menagerie.Component.HTTP.Connection
        Menagerie.Component.HTTP.Types
        Menagerie.Component.HTTP.Config
)