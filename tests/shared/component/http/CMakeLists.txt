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
        Menagerie.Component.Http.Listeners
        Menagerie.Component.Http.Drivers
        Menagerie.Component.Http.Routing
        Menagerie.Component.Http.Connection
        Menagerie.Component.Http.Types
        Menagerie.Component.Http.Config
)