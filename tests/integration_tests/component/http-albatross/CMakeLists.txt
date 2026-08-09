##############################################################################
# Http integration tests — real 127.0.0.1:0 sockets via Boost.Beast clients.
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Albatross.Tcp
        test_http_tcp.cpp
        test_http_drain.cpp
        LINK_LIBS
        ${SHARED_TESTING_TARGET}.Albatross
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Albatross.Tcp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Albatross.Tls
        test_http_tls.cpp
        LINK_LIBS
        ${SHARED_TESTING_TARGET}.Albatross
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Albatross.Tls PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Albatross.Server
        test_http_server_lifecycle.cpp
        test_http_server_observer.cpp
        test_http_run_standalone.cpp
        test_http_server_concurrency.cpp
        test_http_config_wiring.cpp
        LINK_LIBS
        Menagerie.Component.Albatross.Server
        ${SHARED_TESTING_TARGET}.Albatross
        Menagerie::Common::Cuckoo
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Albatross.Server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
