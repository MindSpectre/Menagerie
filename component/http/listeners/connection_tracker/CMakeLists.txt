##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${Http}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${Http}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Listeners.ConnectionTracker
        PUBLIC
        ${Http}.Types.Executor
        Boost::asio
        Menagerie::Common::Beavers
)
##############################################################################
