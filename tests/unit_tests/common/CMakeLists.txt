##############################################################################
# Test Stopwatch benchmark
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Stopwatch
        chrono/stopwatch/test_printing_stopwatch.cpp
        chrono/stopwatch/test_stopwatch.cpp
        chrono/stopwatch/main.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Stopwatch
        PRIVATE
        Menagerie::Common::Chrono
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Thread Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.ThreadPool
        multithread/thread_pool/test_thread_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.ThreadPool
        PRIVATE
        Menagerie::Common::Multithread
        Menagerie::Common::Math
        Menagerie::Common::Chrono
        ${TEST_LIBS}

)
##############################################################################

##############################################################################
# Test Thread Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Disruptor
        multithread/disruptor/test_disruptor.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Disruptor
        PRIVATE
        Menagerie::Common::Multithread
        Menagerie::Common::Math
        Menagerie::Common::Chrono
        ${TEST_LIBS}

)
##############################################################################


##############################################################################
# Test Chameleon (unit)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Chameleon
        chameleon/main.cpp
        chameleon/colors_test.cpp
        chameleon/separators_test.cpp
        chameleon/table_test.cpp
        chameleon/section_test.cpp
        chameleon/box_test.cpp
        chameleon/composition_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Chameleon
        PRIVATE
        Menagerie::Common::Chameleon
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test crow
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Crow
        crow/logger_provider_test.cpp
        crow/entry_tests.cpp
        crow/main.cpp
        crow/logger/file_sink_test.cpp
        crow/logger/console_sink_test.cpp
        crow/logger/logger_ordering_test.cpp
        crow/logger/sink_status_test.cpp
        crow/logger/sink_health_test.cpp
        crow/logger/file_naming_test.cpp
        crow/logger/rotation_test.cpp
        crow/logger/logger_registry_test.cpp
        crow/logger/logger_gate_test.cpp
        crow/prefix_filter_test.cpp
        crow/prefix_integration_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Crow
        PRIVATE
        Menagerie::Common::Crow
        ${TEST_LIBS}
)
##############################################################################


##############################################################################
#  Spider uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Spider
        spider/test_spider.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Spider
        Menagerie::Common::Spider
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Algorithms
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Algorithms
        algorithms/test_sliding_window_sorter.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Algorithms
        PRIVATE
        Menagerie::Common::Algorithms
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Beavers Result (Outcome)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Beavers.Outcome
        beavers/test_outcome.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Beavers.Outcome
        PRIVATE
        Menagerie::Common::Beavers
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Beavers InlineString
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Beavers.Strings.InlineString
        beavers/test_inline_string.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Beavers.Strings.InlineString
        PRIVATE
        Menagerie::Common::Beavers
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test EventCount
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.EventCount
        multithread/event_count/test_event_count.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.EventCount
        PRIVATE
        Menagerie::Common::Multithread
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test EventCount - forced condition_variable fallback path
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.EventCount.Fallback
        multithread/event_count/test_event_count.cpp
)
target_compile_definitions(${UNIT_TESTING_TARGET}.EventCount.Fallback
        PRIVATE
        EVENT_COUNT_FORCE_FALLBACK
)
target_link_libraries(${UNIT_TESTING_TARGET}.EventCount.Fallback
        PRIVATE
        Menagerie::Common::Multithread
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test ResourcePool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.ResourcePool
        multithread/resource_pool/test_resource_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.ResourcePool
        PRIVATE
        Menagerie::Common::Multithread
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test AsyncResourcePool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.AsyncResourcePool
        multithread/resource_pool/test_async_resource_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.AsyncResourcePool
        PRIVATE
        Menagerie::Common::Multithread
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Serialization (ConfigInterface machinery + JSON field codecs)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Serialization
        serialization/test_config_interface.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Serialization
        PRIVATE
        Menagerie::Common::Serialization
        ${TEST_LIBS}
)
##############################################################################

