##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${Albatross}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${Albatross}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Listeners.ConnectionTracker
        PUBLIC
        ${Albatross}.Types.Executor
        Boost::asio
        Menagerie::Common::Beaver
)
##############################################################################
