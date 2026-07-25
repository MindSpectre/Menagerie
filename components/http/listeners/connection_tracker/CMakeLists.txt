##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${HTTP}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${HTTP}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Listeners.ConnectionTracker
        PUBLIC
        ${HTTP}.Types.Executor
        Boost::asio
        Menagerie::Common::Beavers
)
##############################################################################
